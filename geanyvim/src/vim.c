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

#include "config.h"

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <geanyplugin.h>

#define CONF_GROUP "Settings"
#define CONF_VI_MODE "vi_mode"

#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)

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
	KB_TOGGLE_VIM_MODE,
	KB_PERFORM_VIM_COMMAND,
	KB_COUNT
};

struct
{
	GtkWidget *prompt;
	GtkWidget *entry;
	GtkWidget *separator_item;
	GtkWidget *toggle_vi_item;
	GtkWidget *perform_vi_item;

	/* caret style used by Geany we can revert to when disabling vi mode */
	gint default_caret_style;

	/* whether vi mode is enabled or disabled */
	gboolean vi_mode; 
	/* if vi mode is valid for a single command and will be disabled automatically
	 * after performing it */
	gboolean onetime_vi_mode;
	/* if we are in the insert mode or command mode of vi */
	gboolean insert_mode;

	/* the last full search command, including '/' or '?' */
	gchar *search_text;
	/* input accumulated over time (e.g. for commands like 100dd) */
	gchar *accumulator;
} plugin_data =
{
	NULL, NULL, NULL, NULL, NULL, -1, TRUE, FALSE, FALSE, NULL, NULL
};


static gchar *get_current_word(ScintillaObject *sci)
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


static void clamp_cursor_pos(ScintillaObject *sci)
{
	if (plugin_data.insert_mode)
		return;

	gint pos = sci_get_current_position(sci);
	gint start_pos = sci_get_position_from_line(sci, sci_get_current_line(sci));
	gint end_pos = sci_get_line_end_position(sci, sci_get_current_line(sci));
	if (pos == end_pos && pos != start_pos)
		sci_send_command(sci, SCI_CHARLEFT);
}


static void prepare_vi_mode(GeanyDocument *doc)
{
	ScintillaObject *sci;

	if (!doc)
		return;

	sci = doc->editor->sci;

	if (plugin_data.default_caret_style == -1)
		plugin_data.default_caret_style = SSM(sci, SCI_GETCARETSTYLE, 0, 0);

	if (plugin_data.vi_mode)
	{
		if (plugin_data.insert_mode)
			SSM(sci, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
		else
			SSM(sci, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
	}
	else
		SSM(sci, SCI_SETCARETSTYLE, plugin_data.default_caret_style, 0);

	if (plugin_data.vi_mode)
	{
		const gchar *mode = plugin_data.insert_mode ? "INSERT" : "COMMAND";
		ui_set_statusbar(FALSE, "Vim Mode: -- %s --", mode);
	}

	clamp_cursor_pos(sci);
}


static void leave_onetime_vi_mode()
{
	if (plugin_data.onetime_vi_mode)
	{
		plugin_data.vi_mode = FALSE;
		plugin_data.onetime_vi_mode = FALSE;
		plugin_data.insert_mode = FALSE;
		prepare_vi_mode(document_get_current());
	}
}


static void close_prompt()
{
	leave_onetime_vi_mode();
	gtk_widget_hide(plugin_data.prompt);
}


static void perform_search(gboolean forward)
{
	GeanyDocument *doc = document_get_current();
	ScintillaObject *sci = doc != NULL ? doc->editor->sci : NULL;
	struct Sci_TextToFind ttf;
	gint loc, len, pos;

	if (!sci || !plugin_data.search_text)
		return;

	len = sci_get_length(sci);
	pos = sci_get_current_position(sci);

	forward = (plugin_data.search_text[0] == '/' && forward) ||
			(plugin_data.search_text[0] == '?' && !forward);
	ttf.lpstrText = plugin_data.search_text + 1;
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


static void perform_command(const gchar *cmd)
{
	guint i = 0;
	guint len = strlen(cmd);
	GeanyDocument *doc = document_get_current();
	//ScintillaObject *sci = doc != NULL ? doc->editor->sci : NULL;

	if (cmd == NULL || len == 1)
		return;

	switch (cmd[i])
	{
		case ':':
		{
			i++;
			while (i < len)
			{
				switch (cmd[i])
				{
					case 'w':
					{
						if (doc != NULL)
							document_save_file(doc, FALSE);
						break;
					}
				}
				i++;
			}
		}
		case '/':
		case '?':
			g_free(plugin_data.search_text);
			plugin_data.search_text = g_strdup(cmd);
			perform_search(TRUE);
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
			perform_command(gtk_entry_get_text(GTK_ENTRY(plugin_data.entry)));
			close_prompt();
			return TRUE;
	}

	return FALSE;
}


static void on_entry_text_notify(GObject *object, GParamSpec *pspec, gpointer dummy)
{
	const gchar* cmd = gtk_entry_get_text(GTK_ENTRY(plugin_data.entry));

	if (cmd == NULL || strlen(cmd) == 0)
		close_prompt();
}


static void on_prompt_show(GtkWidget *widget, gpointer dummy)
{
	gtk_widget_grab_focus(plugin_data.entry);
}


static gchar *get_config_filename (void)
{
	return g_build_filename (geany_data->app->configdir, "plugins", PLUGIN, PLUGIN".conf", NULL);
}


static void load_config(void)
{
	gchar *filename = get_config_filename();
	GKeyFile *kf = g_key_file_new();

	if (g_key_file_load_from_file(kf, filename, G_KEY_FILE_NONE, NULL))
		plugin_data.vi_mode = g_key_file_get_boolean(kf, CONF_GROUP, CONF_VI_MODE, NULL);
  
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

	g_key_file_set_boolean(kf, CONF_GROUP, CONF_VI_MODE, plugin_data.vi_mode);

	utils_mkdir(dirname, TRUE);
	data = g_key_file_to_data(kf, &length, NULL);
	g_file_set_contents(filename, data, length, NULL);

	g_free(data);
	g_key_file_free(kf);
	g_free(filename);
	g_free(dirname);
}


static void on_toggle_vim_mode(void)
{
	plugin_data.vi_mode = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(plugin_data.toggle_vi_item));
	plugin_data.onetime_vi_mode = FALSE;
	plugin_data.insert_mode = FALSE;
	prepare_vi_mode(document_get_current());
	if (!plugin_data.vi_mode)
		ui_set_statusbar(FALSE, "Vim Mode Disabled");
	save_config();
}


static gboolean on_toggle_vim_mode_kb(GeanyKeyBinding *kb, guint key_id, gpointer data)
{
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(plugin_data.toggle_vi_item),
			!plugin_data.vi_mode);

	return TRUE;
}


static gboolean on_perform_vim_command(GeanyKeyBinding *kb, guint key_id, gpointer data)
{
	if (!plugin_data.vi_mode)
	{
		plugin_data.onetime_vi_mode = TRUE;
		plugin_data.vi_mode = TRUE;
	}
	plugin_data.insert_mode = FALSE;
	prepare_vi_mode(document_get_current());

	return TRUE;
}


static void accumulator_append(const gchar *val)
{
	if (!plugin_data.accumulator)
		plugin_data.accumulator = g_strdup(val);
	else
		SETPTR(plugin_data.accumulator, g_strconcat(plugin_data.accumulator, val, NULL));
}


static void accumulator_clear()
{
	g_free(plugin_data.accumulator);
	plugin_data.accumulator = NULL;
}

static guint accumulator_len()
{
	if (!plugin_data.accumulator)
		return 0;
	return strlen(plugin_data.accumulator);
}


static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	ScintillaObject *sci;

	if (!doc)
		return FALSE;

	sci = doc->editor->sci;

	if (gtk_window_get_focus(GTK_WINDOW(geany->main_widgets->window)) != GTK_WIDGET(sci))
		return FALSE;

	//printf("key: %d, state: %d\n", event->keyval, event->state);

	if (plugin_data.vi_mode)
	{
		gboolean consumed = !plugin_data.insert_mode;

		if (plugin_data.insert_mode)
		{
			if (event->keyval == GDK_KEY_Escape)
			{
				gint pos = sci_get_current_position(sci);
				gint start_pos = sci_get_position_from_line(sci, sci_get_current_line(sci));
				plugin_data.insert_mode = FALSE;
				if (pos > start_pos)
					sci_send_command(sci, SCI_CHARLEFT);
				leave_onetime_vi_mode();
				prepare_vi_mode(doc);
			}
		}
		else
		{
			switch (event->keyval)
			{
				case GDK_KEY_Page_Up:
					sci_send_command(sci, SCI_PAGEUP);
					break;
				case GDK_KEY_Page_Down:
					sci_send_command(sci, SCI_PAGEDOWN);
					break;
				case GDK_KEY_Left:
				case GDK_KEY_leftarrow:
				case GDK_KEY_h:
				{
					gint pos = sci_get_current_position(sci);
					gint start_pos = sci_get_position_from_line(sci, sci_get_current_line(sci));
					if (pos > start_pos)
						sci_send_command(sci, SCI_CHARLEFT);
					break;
				}
				case GDK_KEY_Right:
				case GDK_KEY_rightarrow:
				case GDK_KEY_l:
				{
					gint pos = sci_get_current_position(sci);
					gint end_pos = sci_get_line_end_position(sci, sci_get_current_line(sci));
					if (pos < end_pos - 1)
						sci_send_command(sci, SCI_CHARRIGHT);
					break;
				}
				case GDK_KEY_Down:
				case GDK_KEY_downarrow:
				case GDK_KEY_j:
					sci_send_command(sci, SCI_LINEDOWN);
					clamp_cursor_pos(sci);
					break;
				case GDK_KEY_Up:
				case GDK_KEY_uparrow:
				case GDK_KEY_k:
					sci_send_command(sci, SCI_LINEUP);
					clamp_cursor_pos(sci);
					break;
				case GDK_KEY_colon:
				case GDK_KEY_slash:
				case GDK_KEY_question:
				{
					const gchar *val;
					switch (event->keyval)
					{
						case GDK_KEY_colon:
							val = ":";
							break;
						case GDK_KEY_slash:
							val = "/";
							break;
						default:
							val = "?";
							break;
					}
					gtk_widget_show(plugin_data.prompt);
					gtk_entry_set_text(GTK_ENTRY(plugin_data.entry), val);
					gtk_editable_set_position(GTK_EDITABLE(plugin_data.entry), 1);
					break;
				}
				case GDK_KEY_i:
					plugin_data.insert_mode = TRUE;
					prepare_vi_mode(doc);
					break;
				case GDK_KEY_a:
				{
					gint pos = sci_get_current_position(sci);
					gint end_pos = sci_get_line_end_position(sci, sci_get_current_line(sci));
					if (pos < end_pos)
						sci_send_command(sci, SCI_CHARRIGHT);
					plugin_data.insert_mode = TRUE;
					prepare_vi_mode(doc);
					break;
				}
				case GDK_KEY_n:
					perform_search(TRUE);
					break;
				case GDK_KEY_N:
					perform_search(FALSE);
					break;
				case GDK_KEY_asterisk:
				case GDK_KEY_numbersign:
				{
					gchar *word = get_current_word(sci);
					g_free(plugin_data.search_text);
					if (!word)
						plugin_data.search_text = NULL;
					else
					{
						const gchar *prefix = event->keyval == GDK_KEY_asterisk ? "/" : "?";
						plugin_data.search_text = g_strconcat(prefix, word, NULL);
					}
					g_free(word);
					perform_search(TRUE);
					break;
				}
				case GDK_KEY_u:
				{
					if (SSM(sci, SCI_CANUNDO, 0, 0))
						SSM(sci, SCI_UNDO, 0, 0);
					break;
				}
				case GDK_KEY_r:
				{
					if (event->state & GDK_CONTROL_MASK)
					{
						if (SSM(sci, SCI_CANREDO, 0, 0))
							SSM(sci, SCI_REDO, 0, 0);
					}
					break;
				}
				case GDK_KEY_y:
				{
					guint accum_len = accumulator_len();
					if (accum_len == 0)
						accumulator_append("y");
					else if (accum_len == 1 && plugin_data.accumulator[0] == 'y')
					{
						gint start = sci_get_position_from_line(sci, sci_get_current_line(sci));
						gint end = sci_get_position_from_line(sci, sci_get_current_line(sci)+1);
						SSM(sci, SCI_COPYRANGE, start, end);
						accumulator_clear();
					}
					else
						accumulator_clear();
					break;
				}
				case GDK_KEY_p:
				{	
					gint pos = sci_get_position_from_line(sci, sci_get_current_line(sci)+1);
					sci_set_current_position(sci, pos, TRUE);
					SSM(sci, SCI_PASTE, 0, 0);
					sci_set_current_position(sci, pos, TRUE);
					break;
				}
			}
		}

		return consumed;
	}

	return FALSE;
}


static void on_doc_open(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
		G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);
	prepare_vi_mode(doc);
}


static void on_doc_activate(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
		G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);

	prepare_vi_mode(doc);
}


static gboolean on_editor_notify(GObject *object, GeanyEditor *editor,
		SCNotification *nt, gpointer data)
{
	GeanyDocument *doc = document_get_current();

	/* this makes sure that when we click behind the end of line in command mode,
	 * the cursor is not placed BEHIND the last character but ON the last character */
	if (doc != NULL && nt->nmhdr.code == SCN_UPDATEUI && nt->updated == SC_UPDATE_SELECTION)
		clamp_cursor_pos(doc->editor->sci);

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
	GtkWidget *frame;

	load_config();

	/* menu items and keybindings */
	group = plugin_set_key_group(geany_plugin, "geanyvim", KB_COUNT, NULL);

	plugin_data.separator_item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), plugin_data.separator_item);

	plugin_data.toggle_vi_item = gtk_check_menu_item_new_with_mnemonic(_("Enable _Vim Mode"));
	gtk_widget_show(plugin_data.toggle_vi_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), plugin_data.toggle_vi_item);
	g_signal_connect((gpointer) plugin_data.toggle_vi_item, "activate", G_CALLBACK(on_toggle_vim_mode), NULL);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(plugin_data.toggle_vi_item),
			plugin_data.vi_mode);
	keybindings_set_item_full(group, KB_TOGGLE_VIM_MODE, 0, 0, "toggle_vim",
			_("Enable Vim Mode"), NULL, on_toggle_vim_mode_kb, NULL, NULL);

	plugin_data.perform_vi_item = gtk_menu_item_new_with_mnemonic(_("_Perform Vim Command"));
	gtk_widget_show(plugin_data.perform_vi_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), plugin_data.perform_vi_item);
	g_signal_connect((gpointer) plugin_data.perform_vi_item, "activate", G_CALLBACK(on_perform_vim_command), NULL);
	keybindings_set_item_full(group, KB_PERFORM_VIM_COMMAND, 0, 0, "vim_command",
			_("Perform Vim Command"), NULL, on_perform_vim_command, NULL, NULL);

	/* prompt */
	plugin_data.prompt = g_object_new(GTK_TYPE_WINDOW,
			"decorated", FALSE,
			"default-width", 500,
			"transient-for", geany_data->main_widgets->window,
			"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
			"type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
			"skip-taskbar-hint", TRUE,
			"skip-pager-hint", TRUE,
			NULL);
	g_signal_connect(plugin_data.prompt, "focus-out-event", G_CALLBACK(close_prompt), NULL);
	g_signal_connect(plugin_data.prompt, "show", G_CALLBACK(on_prompt_show), NULL);
	g_signal_connect(plugin_data.prompt, "key-press-event", G_CALLBACK(on_prompt_key_press_event), NULL);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(plugin_data.prompt), frame);
  
	plugin_data.entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(frame), plugin_data.entry);

	g_signal_connect(plugin_data.entry, "notify::text", G_CALLBACK(on_entry_text_notify), NULL);

	gtk_widget_show_all(frame);

	/* final setup */
	prepare_vi_mode(document_get_current());
}


void plugin_cleanup(void)
{
	if (plugin_data.default_caret_style != -1)
	{
		gsize i;
		foreach_document(i)
		{
			ScintillaObject *sci = documents[i]->editor->sci;
			SSM(sci, SCI_SETCARETSTYLE, plugin_data.default_caret_style, 0);
		}
	}

	gtk_widget_destroy(plugin_data.prompt);
	gtk_widget_destroy(plugin_data.separator_item);
	gtk_widget_destroy(plugin_data.toggle_vi_item);
	gtk_widget_destroy(plugin_data.perform_vi_item);

	g_free(plugin_data.search_text);
}


void plugin_help(void)
{
	utils_open_browser("http://plugins.geany.org/geanyvim.html");
}
