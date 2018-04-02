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

#include <gdk/gdkkeysyms.h>

#include "utils.h"


const gchar *kp_to_str(KeyPress *kp)
{
	static gchar *utf8 = NULL;
	gunichar key = gdk_keyval_to_unicode(kp->key);
	gint len;

	if (!utf8)
		utf8 = g_malloc0(MAX_CHAR_SIZE);
	len = g_unichar_to_utf8(key, utf8);
	utf8[len] = '\0';
	return utf8;
}


static gint kp_todigit(KeyPress *kp)
{
	if (kp->modif != 0)
		return -1;

	switch (kp->key)
	{
		case GDK_KEY_0:
		case GDK_KEY_KP_0:
			return 0;
		case GDK_KEY_1:
		case GDK_KEY_KP_1:
			return 1;
		case GDK_KEY_2:
		case GDK_KEY_KP_2:
			return 2;
		case GDK_KEY_3:
		case GDK_KEY_KP_3:
			return 3;
		case GDK_KEY_4:
		case GDK_KEY_KP_4:
			return 4;
		case GDK_KEY_5:
		case GDK_KEY_KP_5:
			return 5;
		case GDK_KEY_6:
		case GDK_KEY_KP_6:
			return 6;
		case GDK_KEY_7:
		case GDK_KEY_KP_7:
			return 7;
		case GDK_KEY_8:
		case GDK_KEY_KP_8:
			return 8;
		case GDK_KEY_9:
		case GDK_KEY_KP_9:
			return 9;
	}
	return -1;
}

gboolean kp_isdigit(KeyPress *kp)
{
	return kp_todigit(kp) != -1;
}


void kpl_printf(GSList *kpl)
{
	kpl = g_slist_reverse(kpl);
	GSList *pos = kpl;
	printf("kpl: ");
	while (pos != NULL)
	{
		KeyPress *kp = pos->data;
		printf("%s<%d>", kp_to_str(kp), kp->key);
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
	gint pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
	gint line = GET_CUR_LINE(sci);
	gint start_pos = SSM(sci, SCI_POSITIONFROMLINE, line, 0);
	gint end_pos = SSM(sci, SCI_GETLINEENDPOSITION, line, 0);
	if (pos == end_pos && pos != start_pos)
		SET_POS_NOX(sci, pos-1, FALSE);
}

static gchar *get_contents_range(ScintillaObject *sci, gint start, gint end)
{
	struct Sci_TextRange tr;
	gchar *text = g_malloc(end - start + 1);

	tr.chrg.cpMin = start;
	tr.chrg.cpMax = end;
	tr.lpstrText = text;

	SSM(sci, SCI_GETTEXTRANGE, 0, (sptr_t)&tr);
	return text;
}

gchar *get_current_word(ScintillaObject *sci)
{
	gint start, end, pos;

	if (!sci)
		return NULL;

	pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
	start = SSM(sci, SCI_WORDSTARTPOSITION, pos, TRUE);
	end = SSM(sci, SCI_WORDENDPOSITION, pos, TRUE);

	if (start == end)
		return NULL;

	return get_contents_range(sci, start, end);
}

void perform_search(CmdContext *c, gint num, gboolean invert)
{
	struct Sci_TextToFind ttf;
	gint pos = SSM(c->sci, SCI_GETCURRENTPOS, 0, 0);
	gint len = SSM(c->sci, SCI_GETLENGTH, 0, 0);
	gboolean forward;
	gint i;

	if (!c->search_text)
		return;

	forward = c->search_text[0] == '/';
	forward = !forward != !invert;
	ttf.lpstrText = c->search_text + 1;

	for (i = 0; i < num; i++)
	{
		gint new_pos;

		if (forward)
		{
			ttf.chrg.cpMin = pos + 1;
			ttf.chrg.cpMax = len;
		}
		else
		{
			ttf.chrg.cpMin = pos;
			ttf.chrg.cpMax = 0;
		}

		new_pos = SSM(c->sci, SCI_FINDTEXT, 0, (sptr_t)&ttf);
		if (new_pos < 0)
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

			new_pos = SSM(c->sci, SCI_FINDTEXT, 0, (sptr_t)&ttf);
		}

		if (new_pos < 0)
			break;
		pos = new_pos;
	}

	if (pos >= 0)
		SET_POS(c->sci, pos, TRUE);
}

void set_current_position(ScintillaObject *sci, gint position, gboolean scroll_to_caret,
	gboolean caretx)
{
	if (scroll_to_caret)
		SSM(sci, SCI_GOTOPOS, (uptr_t) position, 0);
	else
	{
		SSM(sci, SCI_SETCURRENTPOS, (uptr_t) position, 0);
		SSM(sci, SCI_SETANCHOR, (uptr_t) position, 0); /* to avoid creation of a selection */
	}
	if (caretx)
		SSM(sci, SCI_CHOOSECARETX, 0, 0);
}
