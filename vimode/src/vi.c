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

#include "vi.h"
#include "cmds.h"
#include "utils.h"
#include "prompt-ex.h"

struct
{
	/* caret style used by Scintilla we can revert to when disabling vi mode */
	gint default_caret_style;
	/* caret period used by Scintilla we can revert to when disabling vi mode */
	gint default_caret_period;

	/* whether vi mode is enabled or disabled */
	gboolean vim_enabled;
	/* whether insert mode is normal Scintilla ("dummies mode") or normal vim insert mode */
	gboolean insert_for_dummies;

	/* vi mode */
	ViMode vi_mode;
	/* key presses accumulated over time (e.g. for commands like 100dd) */
	GSList *kpl;
	/* kpl of the previous command (used for repeating last command) */
	GSList *prev_kpl;
} state =
{
	-1, -1,
	TRUE, FALSE, FALSE,
	VI_MODE_COMMAND, NULL, NULL
};

CmdContext ctx =
{
	NULL, NULL, FALSE, 0, 1, "", 0, FALSE, NULL, NULL
};


ViMode vi_get_mode(void)
{
	return state.vi_mode;
}


void vi_enter_cmdline_mode()
{
	KeyPress *kp = g_slist_nth_data(state.kpl, 0);
	const gchar *c = kp_to_str(kp);
	gchar *val;
	if (VI_IS_VISUAL(state.vi_mode) && c[0] == ':')
		val = g_strconcat(c, "'<,'>", NULL);
	else
		val = g_strdup(kp_to_str(kp));
	ex_prompt_show(val);
	g_free(val);
}


static void repeat_insert(gboolean replace)
{
	ScintillaObject *sci = ctx.sci;

	if (sci && ctx.num > 1 && ctx.insert_buf_len > 0)
	{
		gint i;

		SSM(sci, SCI_BEGINUNDOACTION, 0, 0);
		for (i = 0; i < ctx.num - 1; i++)
		{
			gint line;
			gint line_len;

			if (ctx.newline_insert)
				SSM(sci, SCI_NEWLINE, 0, 0);

			line = GET_CUR_LINE(sci);
			line_len = SSM(sci, SCI_LINELENGTH, line, 0);

			SSM(sci, SCI_ADDTEXT, ctx.insert_buf_len, (sptr_t) ctx.insert_buf);

			if (replace)
			{
				gint pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
				gint line_end_pos = SSM(sci, SCI_GETLINEENDPOSITION, line, 0);
				gint diff = SSM(sci, SCI_LINELENGTH, line, 0) - line_len;
				diff = pos + diff > line_end_pos ? line_end_pos - pos : diff;
				SSM(sci, SCI_DELETERANGE, pos, diff);
			}
		}
		SSM(sci, SCI_ENDUNDOACTION, 0, 0);
	}
	ctx.num = 1;
	ctx.insert_buf_len = 0;
	ctx.insert_buf[0] = '\0';
	ctx.newline_insert = FALSE;
}


void vi_set_mode(ViMode mode)
{
	ScintillaObject *sci = ctx.sci;
	ViMode prev_mode = state.vi_mode;

	state.vi_mode = mode;

	if (!sci)
		return;

	if (state.default_caret_style == -1)
	{
		state.default_caret_style = SSM(sci, SCI_GETCARETSTYLE, 0, 0);
		state.default_caret_period = SSM(sci, SCI_GETCARETPERIOD, 0, 0);
	}

	if (!state.vim_enabled)
	{
		SSM(sci, SCI_SETCARETSTYLE, state.default_caret_style, 0);
		SSM(sci, SCI_SETCARETPERIOD, state.default_caret_period, 0);
		return;
	}

	if (mode != prev_mode)
		ctx.cb->on_mode_change(mode);

	switch (mode)
	{
		case VI_MODE_COMMAND:
		case VI_MODE_COMMAND_SINGLE:
		{
			gint pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
			if (mode == VI_MODE_COMMAND && VI_IS_INSERT(prev_mode))
			{
				repeat_insert(prev_mode == VI_MODE_REPLACE);

				//repeat_insert() can change current position
				pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
				gint start_pos = SSM(sci, SCI_POSITIONFROMLINE, GET_CUR_LINE(sci), 0);
				if (pos > start_pos)
					SET_POS(sci, PREV(sci, pos), FALSE);
			}
			else if (VI_IS_VISUAL(prev_mode))
				SSM(sci, SCI_SETEMPTYSELECTION, pos, 0);

			SSM(sci, SCI_SETOVERTYPE, 0, 0);
			SSM(sci, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
			SSM(sci, SCI_SETCARETPERIOD, 0, 0);
			SSM(sci, SCI_CANCEL, 0, 0);
			clamp_cursor_pos(sci);
			break;
		}
		case VI_MODE_INSERT:
		case VI_MODE_REPLACE:
			if (mode == VI_MODE_INSERT)
				SSM(sci, SCI_SETOVERTYPE, 0, 0);
			else
				SSM(sci, SCI_SETOVERTYPE, 1, 0);
			SSM(sci, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
			SSM(sci, SCI_SETCARETPERIOD, state.default_caret_period, 0);
			ctx.insert_buf_len = 0;
			ctx.insert_buf[0] = '\0';
			break;
		case VI_MODE_VISUAL:
		case VI_MODE_VISUAL_LINE:
		case VI_MODE_VISUAL_BLOCK:
			SSM(sci, SCI_SETOVERTYPE, 0, 0);
			/* Even with block-style caret, scintilla's caret behaves differently
			 * from how vim behaves - it always behaves as if the caret is before
			 * the character it's placed on. With visual mode we'd need selection
			 * to go behind the caret but we cannot achieve this. Visual mode
			 * simply won't behave as vim's visual mode in this respect. Use
			 * line caret here which makes it more clear what's being selected. */
			SSM(sci, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
			SSM(sci, SCI_SETCARETPERIOD, 0, 0);
			ctx.sel_anchor = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
			break;
	}
}

void vi_set_active_sci(ScintillaObject *sci)
{
	if (ctx.sci && state.default_caret_style != -1)
	{
		SSM(ctx.sci, SCI_SETCARETSTYLE, state.default_caret_style, 0);
		SSM(ctx.sci, SCI_SETCARETPERIOD, state.default_caret_period, 0);
	}

	ctx.sci = sci;
	if (sci)
		vi_set_mode(state.vi_mode);
}


static gboolean is_printable(GdkEventKey *ev)
{
	guint mask = GDK_MODIFIER_MASK & ~(GDK_SHIFT_MASK | GDK_LOCK_MASK);

	if (ev->state & mask)
		return FALSE;

	return g_unichar_isprint(gdk_keyval_to_unicode(ev->keyval));
}


static KeyPress *kp_from_event_key(GdkEventKey *ev)
{
	guint mask = GDK_MODIFIER_MASK & ~(GDK_SHIFT_MASK | GDK_LOCK_MASK | GDK_CONTROL_MASK);
	KeyPress *kp;

	if (ev->state & mask)
		return NULL;

	switch (ev->keyval)
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
	/* We are interested only in Ctrl presses - Alt is not used in Vim and
	 * shift is included in letter capitalisation implicitly. The only case
	 * we are interested in shift is for insert mode shift+arrow keystrokes. */
	switch (ev->keyval)
	{
		case GDK_KEY_Left:
		case GDK_KEY_KP_Left:
		case GDK_KEY_leftarrow:
		case GDK_KEY_Up:
		case GDK_KEY_KP_Up:
		case GDK_KEY_uparrow:
		case GDK_KEY_Right:
		case GDK_KEY_KP_Right:
		case GDK_KEY_rightarrow:
		case GDK_KEY_Down:
		case GDK_KEY_KP_Down:
		case GDK_KEY_downarrow:
			kp->modif = ev->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK);
			break;
		default:
			kp->modif = ev->state & GDK_CONTROL_MASK;
			break;
	}

	return kp;
}


gboolean vi_notify_key_press(GdkEventKey *event)
{
	ScintillaObject *sci = ctx.sci;
	gboolean command_performed = FALSE;
	gboolean is_repeat_command = FALSE;
	gboolean consumed = FALSE;
	ViMode orig_mode = state.vi_mode;
	KeyPress *kp;

	if (!sci || !state.vim_enabled)
		return FALSE;

	kp = kp_from_event_key(event);
	if (!kp)
		return FALSE;

	state.kpl = g_slist_prepend(state.kpl, kp);
	printf("key: %x, state: %d\n", event->keyval, event->state);
	//kpl_printf(state.kpl);
	//kpl_printf(state.prev_kpl);
	if (VI_IS_COMMAND(state.vi_mode) || VI_IS_VISUAL(state.vi_mode))
	{
		if (VI_IS_COMMAND(state.vi_mode))
			command_performed = process_event_cmd_mode(&ctx, state.kpl, state.prev_kpl,
				&is_repeat_command, &consumed);
		else
			command_performed = process_event_vis_mode(&ctx, state.kpl, &consumed);
		consumed = consumed || is_printable(event);
	}
	else //insert, replace mode
	{
		if (!state.insert_for_dummies || kp->key == GDK_KEY_Escape)
			command_performed = process_event_ins_mode(&ctx, state.kpl, &consumed);
	}

	if (command_performed)
	{
		if (is_repeat_command)
			g_slist_free_full(state.kpl, g_free);
		else
		{
			g_slist_free_full(state.prev_kpl, g_free);
			state.prev_kpl = state.kpl;
		}
		state.kpl = NULL;

		if (orig_mode == VI_MODE_COMMAND_SINGLE)
			vi_set_mode(VI_MODE_INSERT);
	}
	else if (!consumed)
	{
		g_free(kp);
		state.kpl = g_slist_delete_link(state.kpl, state.kpl);
	}

	return consumed;
}

gboolean vi_notify_sci(SCNotification *nt)
{
	ScintillaObject *sci = ctx.sci;

	if (!state.vim_enabled || !sci)
		return FALSE;

	if (nt->nmhdr.code == SCN_CHARADDED && VI_IS_INSERT(state.vi_mode))
	{
		gchar buf[MAX_CHAR_SIZE];
		gint len = g_unichar_to_utf8(nt->ch, buf);

		if (ctx.insert_buf_len + len + 1 < INSERT_BUF_LEN)
		{
			gint i;
			for (i = 0; i < len; i++)
			{
				ctx.insert_buf[ctx.insert_buf_len] = buf[i];
				ctx.insert_buf_len++;
			}
			ctx.insert_buf[ctx.insert_buf_len] = '\0';
		}
	}

	if (nt->nmhdr.code == SCN_UPDATEUI && VI_IS_VISUAL(state.vi_mode))
	{
		if (state.vi_mode == VI_MODE_VISUAL)
		{
			gint anchor = SSM(sci, SCI_GETANCHOR, 0, 0);
			if (anchor != ctx.sel_anchor)
				SSM(sci, SCI_SETANCHOR, ctx.sel_anchor, 0);
		}
		else if (state.vi_mode == VI_MODE_VISUAL_LINE)
		{
			gint pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
			gint anchor_line = SSM(sci, SCI_LINEFROMPOSITION, ctx.sel_anchor, 0);
			gint pos_line = SSM(sci, SCI_LINEFROMPOSITION, pos, 0);
			gint anchor_linepos, pos_linepos;

			if (pos_line >= anchor_line)
			{
				anchor_linepos = SSM(sci, SCI_POSITIONFROMLINE, anchor_line, 0);
				pos_linepos = SSM(sci, SCI_GETLINEENDPOSITION, pos_line, 0);
			}
			else
			{
				anchor_linepos = SSM(sci, SCI_GETLINEENDPOSITION, anchor_line, 0);
				pos_linepos = SSM(sci, SCI_POSITIONFROMLINE, pos_line, 0);
			}

			/* Scintilla' selection spans from anchor position to caret position.
			 * This means that unfortunately we have to set the caret position as
			 * well even though it would be better to have caret independent of
			 * selection like in vim. TODO: explore the possibility of using
			 * multiple selections to simulate this behavior */
			if (SSM(sci, SCI_GETANCHOR, 0, 0) != anchor_linepos || pos != pos_linepos)
				SSM(sci, SCI_SETSEL, anchor_linepos, pos_linepos);
		}
	}

	/* This makes sure that when we click behind the end of line in command mode,
	 * the cursor is not placed BEHIND the last character but ON the last character.
	 * We want to ignore this when doing selection with mouse as it breaks things. */
	if (VI_IS_COMMAND(state.vi_mode) &&
		nt->nmhdr.code == SCN_UPDATEUI && nt->updated == SC_UPDATE_SELECTION &&
		SSM(sci, SCI_GETSELECTIONEND, 0, 0) - SSM(sci, SCI_GETSELECTIONSTART, 0, 0) == 0)
		clamp_cursor_pos(sci);

	return FALSE;
}


void vi_set_enabled(gboolean enabled)
{
	ViMode mode = enabled ? VI_MODE_COMMAND : VI_MODE_INSERT;
	state.vim_enabled = enabled;
	vi_set_mode(mode);
}

void vi_set_insert_for_dummies(gboolean enabled)
{
	state.insert_for_dummies = enabled;
}

gboolean vi_get_enabled(void)
{
	return state.vim_enabled;
}

gboolean vi_get_insert_for_dummies(void)
{
	return state.insert_for_dummies;
}

static void init_cb(ViCallback *cb)
{
	g_assert(cb->on_mode_change && cb->on_save && cb->on_save_all && cb->on_quit);

	ctx.cb = cb;
}

void vi_init(GtkWidget *parent_window, ViCallback *cb)
{
	init_cb(cb);
	ex_prompt_init(parent_window, &ctx);
}

void vi_cleanup(void)
{
	vi_set_active_sci(NULL);
	ex_prompt_cleanup();
	g_free(ctx.search_text);
	g_free(ctx.search_char);
}
