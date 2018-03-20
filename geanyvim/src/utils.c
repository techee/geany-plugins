/*
 * Copyright 2018 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <gdk/gdkkeysyms.h>

#include "utils.h"

ScintillaObject *get_current_doc_sci(void)
{
	GeanyDocument *doc = document_get_current();
	return doc != NULL ? doc->editor->sci : NULL;
}

gchar kp_to_char(KeyPress *kp)
{
	gchar utf8[5];
	gunichar key = gdk_keyval_to_unicode(kp->key);
	g_unichar_to_utf8(key, utf8);
	return utf8[0];
}

static gint kp_todigit(KeyPress *kp)
{
	switch (kp->key)
	{
		case GDK_KEY_0:
			return 0;
		case GDK_KEY_1:
			return 1;
		case GDK_KEY_2:
			return 2;
		case GDK_KEY_3:
			return 3;
		case GDK_KEY_4:
			return 4;
		case GDK_KEY_5:
			return 5;
		case GDK_KEY_6:
			return 6;
		case GDK_KEY_7:
			return 7;
		case GDK_KEY_8:
			return 8;
		case GDK_KEY_9:
			return 9;
	}
	return -1;
}

gboolean kp_isdigit(KeyPress *kp)
{
	return kp_todigit(kp) != -1;
}

KeyPress *kp_from_event_key(GdkEventKey *ev)
{
	KeyPress *kp;
	guint k = ev->keyval;

	switch (k)
	{
		case GDK_KEY_Shift_L:
		case GDK_KEY_Shift_R:
		case GDK_KEY_Control_L: 
		case GDK_KEY_Control_R:
		case GDK_KEY_Caps_Lock:
		case GDK_KEY_Shift_Lock:
		case GDK_KEY_Meta_L:
		case GDK_KEY_Meta_R:
		case GDK_KEY_Alt_L:
		case GDK_KEY_Alt_R:
		case GDK_KEY_Super_L:
		case GDK_KEY_Super_R:
		case GDK_KEY_Hyper_L:
		case GDK_KEY_Hyper_R:
			return NULL;
	}

	kp = g_new0(KeyPress, 1);
	kp->key = ev->keyval;
	/* we are interested only in Ctrl presses - Alt is not used in Vim and
	 * shift is included in letter capitalisation implicitly */
	kp->modif = ev->state & GDK_CONTROL_MASK;

	return kp;
}

static KeyPress *kpl_copy_elem(KeyPress *src, gpointer data)
{
	KeyPress *kp = g_new0(KeyPress, 1);
	kp->key = src->key;
	kp->modif = src->modif;
	return kp;
}

GSList *kpl_copy(GSList *kpl)
{
	return g_slist_copy_deep(kpl, (GCopyFunc)kpl_copy_elem, NULL);
}

void kpl_printf(GSList *kpl)
{
	kpl = g_slist_reverse(kpl);
	GSList *pos = kpl;
	printf("kpl: ");
	while (pos != NULL)
	{
		KeyPress *kp = pos->data;
		printf("%c<%d>", kp_to_char(kp), kp->key);
		pos = g_slist_next(pos);
	}
	printf("\n");
	kpl = g_slist_reverse(kpl);
}

gint kpl_get_int(GSList *kpl, GSList **new_kpl)
{
	gint res = 0;
	gint i = 0;
	GSList *pos = kpl;
	GSList *num_list = NULL;

	if (new_kpl != NULL)
		*new_kpl = kpl;

	while (pos != NULL)
	{
		if (kp_isdigit(pos->data))
			num_list = g_slist_prepend(num_list, pos->data);
		else
			break;
		pos = g_slist_next(pos);
	}

	if (!num_list)
		return -1;

	if (new_kpl != NULL)
		*new_kpl = pos;

	pos = num_list;
	while (pos != NULL)
	{
		res = res * 10 + kp_todigit(pos->data);
		pos = g_slist_next(pos);
		i++;
		// some sanity check
		if (res > 1000000)
			break;
	}

	return res;
}

void clamp_cursor_pos(ScintillaObject *sci)
{
	gint pos = sci_get_current_position(sci);
	gint start_pos = sci_get_position_from_line(sci, sci_get_current_line(sci));
	gint end_pos = sci_get_line_end_position(sci, sci_get_current_line(sci));
	if (pos == end_pos && pos != start_pos)
		sci_send_command(sci, SCI_CHARLEFT);
}

gchar *get_current_word(ScintillaObject *sci)
{
	gint start, end, pos;

	if (!sci)
		return NULL;

	pos = sci_get_current_position(sci);
	SSM(sci, SCI_WORDSTARTPOSITION, pos, TRUE);
	start = SSM(sci, SCI_WORDSTARTPOSITION, pos, TRUE);
	end = SSM(sci, SCI_WORDENDPOSITION, pos, TRUE);

	if (start == end)
		return NULL;

	if (end - start >= GEANY_MAX_WORD_LENGTH)
		end = start + GEANY_MAX_WORD_LENGTH - 1;
	return sci_get_contents_range(sci, start, end);
}

void perform_search(ScintillaObject *sci, CmdContext *ctx, gboolean forward)
{
	struct Sci_TextToFind ttf;
	gint loc, len, pos;

	if (!ctx->search_text)
		return;

	len = sci_get_length(sci);
	pos = sci_get_current_position(sci);

	forward = (ctx->search_text[0] == '/' && forward) ||
			(ctx->search_text[0] == '?' && !forward);
	ttf.lpstrText = ctx->search_text + 1;
	if (forward)
	{
		ttf.chrg.cpMin = pos + 1;
		ttf.chrg.cpMax = len;
	}
	else
	{
		ttf.chrg.cpMin = pos - 1;
		ttf.chrg.cpMax = 0;
	}

	loc = sci_find_text(sci, 0, &ttf);
	if (loc < 0)
	{
		/* wrap */
		if (forward)
		{
			ttf.chrg.cpMin = 0;
			ttf.chrg.cpMax = pos;
		}
		else
		{
			ttf.chrg.cpMin = len;
			ttf.chrg.cpMax = pos;
		}

		loc = sci_find_text(sci, 0, &ttf);
	}

	if (loc >= 0)
		sci_set_current_position(sci, loc, TRUE);
}

