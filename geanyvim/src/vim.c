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

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "vim.h"
#include "cmds.h"
#include "utils.h"
#include "backend-geany.h"

struct
{
	GtkWidget *prompt;
	GtkWidget *entry;
} vi_widgets =
{
	NULL, NULL
};

struct
{
	/* caret style used by Scintilla we can revert to when disabling vi mode */
	gint default_caret_style;
	/* caret period used by Scintilla we can revert to when disabling vi mode */
	gint default_caret_period;

	/* whether vi mode is enabled or disabled */
	gboolean vim_enabled;
	/* whether insert mode should be used by default when loading the plugin */
	gboolean start_in_insert;
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
	NULL, NULL, FALSE, 0, 1, "", 0
};


ViMode get_vi_mode(void)
{
	return state.vi_mode;
}

void enter_cmdline_mode()
{
	KeyPress *kp = g_slist_nth_data(state.kpl, 0);
	const gchar *val = kp_to_str(kp);
	gtk_widget_show(vi_widgets.prompt);
	gtk_entry_set_text(GTK_ENTRY(vi_widgets.entry), val);
	gtk_editable_set_position(GTK_EDITABLE(vi_widgets.entry), 1);
}


const gchar *get_inserted_text(void)
{
	ctx.insert_buf[ctx.insert_buf_len] = '\0';
	return ctx.insert_buf;
}

static void repeat_insert(gboolean replace)
{
	if (ctx.num > 1 && ctx.insert_buf_len > 0)
	{
		ScintillaObject *sci = get_current_doc_sci();
		gint i;

		ctx.insert_buf[ctx.insert_buf_len] = '\0';

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
	ctx.newline_insert = FALSE;
}

static void set_vi_mode_full(ViMode mode, ScintillaObject *sci)
{
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
		on_mode_change(mode);

	switch (mode)
	{
		case VI_MODE_COMMAND:
		case VI_MODE_COMMAND_SINGLE:
		{
			gint pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
			if (mode == VI_MODE_COMMAND && IS_INSERT(prev_mode))
			{
				repeat_insert(prev_mode == VI_MODE_REPLACE);

				//repeat_insert() can change current position
				pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
				gint start_pos = SSM(sci, SCI_POSITIONFROMLINE, GET_CUR_LINE(sci), 0);
				if (pos > start_pos)
					SET_POS(sci, PREV(sci, pos), FALSE);
			}
			else if (IS_VISUAL(prev_mode))
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

void vim_sc_init(ScintillaObject *sci)
{
	set_vi_mode_full(state.vi_mode, sci);
}

void vim_sc_cleanup(ScintillaObject *sci)
{
	if (state.default_caret_style != -1)
	{
		SSM(sci, SCI_SETCARETSTYLE, state.default_caret_style, 0);
		SSM(sci, SCI_SETCARETPERIOD, state.default_caret_period, 0);
	}
}

void set_vi_mode(ViMode mode)
{
	set_vi_mode_full(mode, get_current_doc_sci());
}


static void close_prompt()
{
	gtk_widget_hide(vi_widgets.prompt);
}


static void perform_command(const gchar *cmd)
{
	guint len = strlen(cmd);
	ScintillaObject *sci = get_current_doc_sci();

	if (!sci)
		return;

	if (cmd == NULL || len == 0)
		return;

	switch (cmd[0])
	{
		case ':':
		{
			const gchar *c = cmd + 1;
			if (strcmp(c, "w") || strcmp(c, "w!") || strcmp(c, "write") || strcmp(c, "write!"))
				on_save();
			else if (strcmp(c, "wall") || strcmp(c, "wall!"))
				on_save_all();
			break;
		}
		case '/':
		case '?':
			if (len == 1)
			{
				if (ctx.search_text && strlen(ctx.search_text) > 1)
					ctx.search_text[0] = cmd[0];
			}
			else
			{
				g_free(ctx.search_text);
				ctx.search_text = g_strdup(cmd);
			}
			perform_search(sci, &ctx, ctx.num, FALSE);
			break;
	}
}


static gboolean on_prompt_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer dummy)
{
	switch (event->keyval)
	{
		case GDK_KEY_Escape:
			close_prompt();
			return TRUE;

		case GDK_KEY_Tab:
			/* avoid leaving the entry */
			return TRUE;

		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_ISO_Enter:
			perform_command(gtk_entry_get_text(GTK_ENTRY(vi_widgets.entry)));
			close_prompt();
			return TRUE;
	}

	return FALSE;
}


static void on_entry_text_notify(GObject *object, GParamSpec *pspec, gpointer dummy)
{
	const gchar* cmd = gtk_entry_get_text(GTK_ENTRY(vi_widgets.entry));

	if (cmd == NULL || strlen(cmd) == 0)
		close_prompt();
}


static void on_prompt_show(GtkWidget *widget, gpointer dummy)
{
	gtk_widget_grab_focus(vi_widgets.entry);
}

gboolean on_key_press_notification(GdkEventKey *event)
{
	ScintillaObject *sci = get_current_doc_sci();
	gboolean command_performed = FALSE;
	gboolean is_repeat_command = FALSE;
	gboolean consumed = FALSE;
	ViMode orig_mode = state.vi_mode;
	KeyPress *kp;

	if (!state.vim_enabled)
		return FALSE;

	kp = kp_from_event_key(event);
	if (!kp)
		return FALSE;

	state.kpl = g_slist_prepend(state.kpl, kp);
	printf("key: %x, state: %d\n", event->keyval, event->state);
	//kpl_printf(state.kpl);
	//kpl_printf(state.prev_kpl);
	if (IS_COMMAND(state.vi_mode) || IS_VISUAL(state.vi_mode))
	{
		if (IS_COMMAND(state.vi_mode))
			command_performed = process_event_cmd_mode(sci, &ctx, state.kpl, state.prev_kpl,
				&is_repeat_command, &consumed);
		else
			command_performed = process_event_vis_mode(sci, &ctx, state.kpl, &consumed);
		consumed = consumed || is_printable(event);
	}
	else //insert, replace mode
	{
		if (!state.insert_for_dummies || kp->key == GDK_KEY_Escape)
			command_performed = process_event_ins_mode(sci, &ctx, state.kpl, &consumed);
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
			set_vi_mode(VI_MODE_INSERT);
	}
	else if (!consumed)
	{
		g_free(kp);
		state.kpl = g_slist_delete_link(state.kpl, state.kpl);
	}

	return consumed;
}

gboolean on_sc_notification(SCNotification *nt)
{
	ScintillaObject *sci = get_current_doc_sci();

	if (!state.vim_enabled || !sci)
		return FALSE;

	if (nt->nmhdr.code == SCN_CHARADDED && IS_INSERT(state.vi_mode))
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
		}
	}

	if (nt->nmhdr.code == SCN_UPDATEUI && IS_VISUAL(state.vi_mode))
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

	//if (nt->nmhdr.code == SCN_MODIFIED && nt->modificationType & SC_MOD_CONTAINER)
	//{
	//	printf("token: %d\n", nt->token);
	//	SET_POS(sci, nt->token, TRUE);
	//}

	//if (SSM(sci, SCI_GETSELTEXT, 0, 0) > 0)
	//	return FALSE;

	/* this makes sure that when we click behind the end of line in command mode,
	 * the cursor is not placed BEHIND the last character but ON the last character */
	//if (nt->nmhdr.code == SCN_UPDATEUI && nt->updated == SC_UPDATE_SELECTION)
	//	clamp_cursor_pos(sci);

	return FALSE;
}


void set_vim_enabled(gboolean enabled)
{
	state.vim_enabled = enabled;
}

void set_start_in_insert(gboolean enabled)
{
	state.start_in_insert = enabled;
}

void set_insert_for_dummies(gboolean enabled)
{
	state.insert_for_dummies = enabled;
}

gboolean get_vim_enabled(void)
{
	return state.vim_enabled;
}

gboolean get_start_in_insert(void)
{
	return state.start_in_insert;
}

gboolean get_insert_for_dummies(void)
{
	return state.insert_for_dummies;
}

void vim_init(GtkWidget *window)
{
	GtkWidget *frame;

	/* prompt */
	vi_widgets.prompt = g_object_new(GTK_TYPE_WINDOW,
			"decorated", FALSE,
			"default-width", 500,
			"transient-for", window,
			"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
			"type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
			"skip-taskbar-hint", TRUE,
			"skip-pager-hint", TRUE,
			NULL);
	g_signal_connect(vi_widgets.prompt, "focus-out-event", G_CALLBACK(close_prompt), NULL);
	g_signal_connect(vi_widgets.prompt, "show", G_CALLBACK(on_prompt_show), NULL);
	g_signal_connect(vi_widgets.prompt, "key-press-event", G_CALLBACK(on_prompt_key_press_event), NULL);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(vi_widgets.prompt), frame);
  
	vi_widgets.entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(frame), vi_widgets.entry);

	g_signal_connect(vi_widgets.entry, "notify::text", G_CALLBACK(on_entry_text_notify), NULL);

	gtk_widget_show_all(frame);

	set_vi_mode(state.start_in_insert ? VI_MODE_INSERT : VI_MODE_COMMAND);
}

void vim_cleanup(void)
{
	gtk_widget_destroy(vi_widgets.prompt);
	g_free(ctx.search_text);
	g_free(ctx.search_char);
}
