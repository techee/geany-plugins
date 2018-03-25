/*
 * Copyright (C) 2018 Jiri Techet <techet@gmail.com>
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

#include <geanyplugin.h>

#include "state.h"
#include "cmds.h"
#include "utils.h"

#define CONF_GROUP "Settings"
#define CONF_ENABLE_VIM "enable_vim"
#define CONF_START_IN_INSERT "start_in_insert"
#define CONF_INSERT_FOR_DUMMIES "insert_for_dummies"

GeanyPlugin *geany_plugin;
GeanyData *geany_data;

PLUGIN_VERSION_CHECK(235)

PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("Geany Vim"),
	_("Vim mode for Geany"),
	VERSION,
	"Jiří Techet <techet@gmail.com>"
)

enum
{
	KB_ENABLE_VIM,
	KB_START_IN_INSERT,
	KB_INSERT_FOR_DUMMIES,
	KB_COUNT
};

struct
{
	GtkWidget *prompt;
	GtkWidget *entry;
	GtkWidget *parent_item;
	GtkWidget *enable_vim_item;
	GtkWidget *start_in_insert_item;
	GtkWidget *insert_for_dummies_item;
} vi_widgets =
{
	NULL, NULL, NULL, NULL, NULL, NULL
};

struct
{
	/* caret style used by Geany we can revert to when disabling vi mode */
	gint default_caret_style;
	/* caret period used by Geany we can revert to when disabling vi mode */
	gint default_caret_period;

	/* whether vi mode is enabled or disabled */
	gboolean vim_enabled;
	/* whether insert mode should be used by default when loading the plugin */
	gboolean start_in_insert;
	/* whether insert mode is normal Geany ("dummies mode") or normal vim insert mode */
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


static const gchar *get_mode_name(ViMode vi_mode)
{
	switch (vi_mode)
	{
		case VI_MODE_COMMAND:
			return "COMMAND";
			break;
		case VI_MODE_COMMAND_SINGLE:
			return "(insert)";
			break;
		case VI_MODE_INSERT:
			return "INSERT";
			break;
		case VI_MODE_REPLACE:
			return "REPLACE";
			break;
		case VI_MODE_VISUAL:
			return "VISUAL";
			break;
		case VI_MODE_VISUAL_LINE:
			return "VISUAL LINE";
			break;
		case VI_MODE_VISUAL_BLOCK:
			return "VISUAL BLOCK";
			break;
	}
	return "";
}

ViMode get_vi_mode(void)
{
	return state.vi_mode;
}

void enter_cmdline_mode()
{
	KeyPress *kp = g_slist_nth_data(state.kpl, 0);
	gchar val[2] = {kp_to_char(kp), '\0'};
	gtk_widget_show(vi_widgets.prompt);
	gtk_entry_set_text(GTK_ENTRY(vi_widgets.entry), val);
	gtk_editable_set_position(GTK_EDITABLE(vi_widgets.entry), 1);
}

static ScintillaObject *get_current_doc_sci(void)
{
	GeanyDocument *doc = document_get_current();
	return doc != NULL ? doc->editor->sci : NULL;
}

const gchar *get_inserted_text(void)
{
	ctx.insert_buf[ctx.insert_buf_len] = '\0';
	return ctx.insert_buf;
}

static void repeat_insert(void)
{
	if ((state.vi_mode == VI_MODE_INSERT || state.vi_mode == VI_MODE_REPLACE) &&
		ctx.num > 1 && ctx.insert_buf_len > 0)
	{
		ScintillaObject *sci = get_current_doc_sci();
		gint i;

		ctx.insert_buf[ctx.insert_buf_len] = '\0';

		sci_start_undo_action(sci);
		/* insert newline for 'o' and 'O' insert modes - this is recorded as
		 * a normal keystroke and added into insert_buf so we don't have to
		 * add newlines in the cycle below */
		if (ctx.newline_insert)
			SSM(sci, SCI_NEWLINE, 0, 0);
		for (i = 0; i < ctx.num - 1; i++)
			SSM(sci, SCI_ADDTEXT, ctx.insert_buf_len, (sptr_t) ctx.insert_buf);
		sci_end_undo_action(sci);
	}
	ctx.num = 1;
	ctx.insert_buf_len = 0;
	ctx.newline_insert = FALSE;
}

static void set_vi_mode_full(ViMode mode, ScintillaObject *sci)
{
	if (!sci)
	{
		state.vi_mode = mode;
		return;
	}

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

	if (mode != state.vi_mode)
		ui_set_statusbar(FALSE, "Vim Mode: -- %s --", get_mode_name(mode));

	switch (mode)
	{
		case VI_MODE_COMMAND:
		case VI_MODE_COMMAND_SINGLE:
		{
			gint pos = sci_get_current_position(sci);
			if (state.vi_mode == VI_MODE_COMMAND &&
				(state.vi_mode == VI_MODE_INSERT || state.vi_mode == VI_MODE_REPLACE))
			{
				repeat_insert();

				//repeat_insert() can change current position
				pos = sci_get_current_position(sci);
				gint start_pos = sci_get_position_from_line(sci, sci_get_current_line(sci));
				if (pos > start_pos)
					sci_set_current_position(sci, PREV(sci, pos), FALSE);
			}
			else if (IS_VISUAL(state.vi_mode))
				SSM(sci, SCI_SETEMPTYSELECTION, pos, 0);

			g_slist_free_full(state.kpl, g_free);
			state.kpl = NULL;

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
			ctx.sel_anchor = sci_get_current_position(sci);
			break;
	}

	state.vi_mode = mode;
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
	GeanyDocument *doc = document_get_current();
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
			{
				if (doc != NULL)
					document_save_file(doc, FALSE);
			}
			else if (strcmp(c, "wall") || strcmp(c, "wall!"))
			{
				gint i;
				foreach_document(i)
				{
					document_save_file(documents[i], FALSE);
				}
			}
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


static gchar *get_config_filename(void)
{
	return g_build_filename(geany_data->app->configdir, "plugins", PLUGIN, PLUGIN".conf", NULL);
}


static void load_config(void)
{
	gchar *filename = get_config_filename();
	GKeyFile *kf = g_key_file_new();

	if (g_key_file_load_from_file(kf, filename, G_KEY_FILE_NONE, NULL))
	{
		state.vim_enabled = utils_get_setting_boolean(kf, CONF_GROUP, CONF_ENABLE_VIM, TRUE);
		state.start_in_insert = utils_get_setting_boolean(kf,
			CONF_GROUP, CONF_START_IN_INSERT, FALSE);
		state.insert_for_dummies = utils_get_setting_boolean(kf,
			CONF_GROUP, CONF_INSERT_FOR_DUMMIES, FALSE);
	}
  
	g_key_file_free(kf);
	g_free(filename);
}


static void save_config(void)
{
	GKeyFile *kf = g_key_file_new();
	gchar *filename = get_config_filename();
	gchar *dirname = g_path_get_dirname(filename);
	gchar *data;
	gsize length;

	g_key_file_set_boolean(kf, CONF_GROUP, CONF_ENABLE_VIM, state.vim_enabled);
	g_key_file_set_boolean(kf, CONF_GROUP, CONF_START_IN_INSERT, state.start_in_insert);
	g_key_file_set_boolean(kf, CONF_GROUP, CONF_INSERT_FOR_DUMMIES, state.insert_for_dummies);

	utils_mkdir(dirname, TRUE);
	data = g_key_file_to_data(kf, &length, NULL);
	g_file_set_contents(filename, data, length, NULL);

	g_free(data);
	g_key_file_free(kf);
	g_free(filename);
	g_free(dirname);
}


static void on_enable_vim_mode(void)
{
	state.vim_enabled = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(vi_widgets.enable_vim_item));
	set_vi_mode(state.start_in_insert ? VI_MODE_INSERT : VI_MODE_COMMAND);
	if (!state.vim_enabled)
		ui_set_statusbar(FALSE, "Vim Mode Disabled");
	save_config();
}


static gboolean on_enable_vim_mode_kb(GeanyKeyBinding *kb, guint key_id, gpointer data)
{
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(vi_widgets.enable_vim_item),
			!state.vim_enabled);
	return TRUE;
}


static void on_start_in_insert(void)
{
	state.start_in_insert =
		gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(vi_widgets.start_in_insert_item));
	save_config();
}


static void on_insert_for_dummies(void)
{
	state.insert_for_dummies =
		gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(vi_widgets.insert_for_dummies_item));
	ui_set_statusbar(FALSE, _("Insert Mode for Dummies: %s"), state.insert_for_dummies ? _("ON") : _("OFF"));
	save_config();
}

static gboolean on_insert_for_dummies_kb(GeanyKeyBinding *kb, guint key_id, gpointer data)
{
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(vi_widgets.insert_for_dummies_item),
			!state.insert_for_dummies);
	return TRUE;
}


static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	ScintillaObject *sci = get_current_doc_sci();
	gboolean command_performed = FALSE;
	gboolean is_repeat_command = FALSE;
	gboolean consumed = FALSE;
	ViMode orig_mode = state.vi_mode;
	KeyPress *kp;

	if (!state.vim_enabled || !sci ||
		gtk_window_get_focus(GTK_WINDOW(geany->main_widgets->window)) != GTK_WIDGET(sci))
		return FALSE;

	kp = kp_from_event_key(event);
	if (!kp)
		return FALSE;

	if (IS_COMMAND(state.vi_mode) || IS_VISUAL(state.vi_mode))
	{
		state.kpl = g_slist_prepend(state.kpl, kp);
		printf("key: %x, state: %d\n", event->keyval, event->state);
		//kpl_printf(state.kpl);
		//kpl_printf(state.prev_kpl);
		if (IS_COMMAND(state.vi_mode))
			command_performed = process_event_cmd_mode(sci, &ctx, state.kpl, state.prev_kpl,
				&is_repeat_command, &consumed);
		else
			command_performed = process_event_vis_mode(sci, &ctx, state.kpl, &consumed);
		consumed = consumed || is_printable(event);
	}
	else //insert, replace mode
	{
		if (!is_printable(event) && (!state.insert_for_dummies || kp->key == GDK_KEY_Escape))
		{
			state.kpl = g_slist_prepend(state.kpl, kp);
			command_performed = process_event_ins_mode(sci, &ctx, state.kpl, &consumed);
		}
		else
		{
			g_free(kp);
			kp = NULL;
		}
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

	return consumed;
}


static void on_doc_open(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
		G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);
	set_vi_mode_full(state.vi_mode, doc->editor->sci);
}


static void on_doc_activate(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
		G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);
	set_vi_mode_full(state.vi_mode, doc->editor->sci);
}


static gboolean on_editor_notify(GObject *object, GeanyEditor *editor,
		SCNotification *nt, gpointer data)
{
	ScintillaObject *sci = get_current_doc_sci();

	if (!state.vim_enabled || !sci)
		return FALSE;

	if (nt->nmhdr.code == SCN_CHARADDED &&
		(state.vi_mode == VI_MODE_INSERT || state.vi_mode == VI_MODE_REPLACE))
	{
		gchar buf[7];
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
			gint pos = sci_get_current_position(sci);
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
	//	sci_set_current_position(sci, nt->token, TRUE);
	//}

	//if (sci_get_selected_text_length(sci) > 0)
	//	return FALSE;

	/* this makes sure that when we click behind the end of line in command mode,
	 * the cursor is not placed BEHIND the last character but ON the last character */
	//if (nt->nmhdr.code == SCN_UPDATEUI && nt->updated == SC_UPDATE_SELECTION)
	//	clamp_cursor_pos(sci);

	return FALSE;
}


PluginCallback plugin_callbacks[] = {
	{"document-open", (GCallback) &on_doc_open, TRUE, NULL},
	{"document-activate", (GCallback) &on_doc_activate, TRUE, NULL},
	{"editor-notify", (GCallback) &on_editor_notify, TRUE, NULL},
	{"key-press", (GCallback) &on_key_press, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};


void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *group;
	GtkWidget *frame, *menu;

	load_config();

	/* menu items and keybindings */
	group = plugin_set_key_group(geany_plugin, "geanyvim", KB_COUNT, NULL);

	vi_widgets.parent_item = gtk_menu_item_new_with_mnemonic(_("_Vim Mode"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), vi_widgets.parent_item);

	menu = gtk_menu_new ();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(vi_widgets.parent_item), menu);

	vi_widgets.enable_vim_item = gtk_check_menu_item_new_with_mnemonic(_("Enable _Vim Mode"));
	gtk_container_add(GTK_CONTAINER(menu), vi_widgets.enable_vim_item);
	g_signal_connect((gpointer) vi_widgets.enable_vim_item, "activate", G_CALLBACK(on_enable_vim_mode), NULL);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(vi_widgets.enable_vim_item), state.vim_enabled);
	keybindings_set_item_full(group, KB_ENABLE_VIM, 0, 0, "enable_vim",
			_("Enable Vim Mode"), NULL, on_enable_vim_mode_kb, NULL, NULL);

	vi_widgets.start_in_insert_item = gtk_check_menu_item_new_with_mnemonic(_("Start in _Insert Mode"));
	gtk_container_add(GTK_CONTAINER(menu), vi_widgets.start_in_insert_item);
	g_signal_connect((gpointer) vi_widgets.start_in_insert_item, "activate",
		G_CALLBACK(on_start_in_insert), NULL);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(vi_widgets.start_in_insert_item), state.start_in_insert);

	vi_widgets.insert_for_dummies_item = gtk_check_menu_item_new_with_mnemonic(_("Insert Mode for _Dummies"));
	gtk_container_add(GTK_CONTAINER(menu), vi_widgets.insert_for_dummies_item);
	g_signal_connect((gpointer) vi_widgets.insert_for_dummies_item, "activate",
		G_CALLBACK(on_insert_for_dummies), NULL);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(vi_widgets.insert_for_dummies_item), state.insert_for_dummies);
	keybindings_set_item_full(group, KB_START_IN_INSERT, 0, 0, "insert_for_dummies",
			_("Insert Mode for Dummies"), NULL, on_insert_for_dummies_kb, NULL, NULL);

	gtk_widget_show_all(vi_widgets.parent_item);

	/* prompt */
	vi_widgets.prompt = g_object_new(GTK_TYPE_WINDOW,
			"decorated", FALSE,
			"default-width", 500,
			"transient-for", geany_data->main_widgets->window,
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


void plugin_cleanup(void)
{
	if (state.default_caret_style != -1)
	{
		gsize i;
		foreach_document(i)
		{
			ScintillaObject *sci = documents[i]->editor->sci;
			SSM(sci, SCI_SETCARETSTYLE, state.default_caret_style, 0);
			SSM(sci, SCI_SETCARETPERIOD, state.default_caret_period, 0);
		}
	}

	gtk_widget_destroy(vi_widgets.prompt);
	gtk_widget_destroy(vi_widgets.parent_item);

	g_free(ctx.search_text);
}


void plugin_help(void)
{
	utils_open_browser("http://plugins.geany.org/geanyvim.html");
}
