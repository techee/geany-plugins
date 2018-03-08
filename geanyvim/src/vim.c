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
} plugin_data =
{
	NULL, NULL, NULL, NULL, NULL, -1, TRUE, FALSE, FALSE
};


static gboolean on_prompt_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer dummy)
{
	switch (event->keyval)
	{
		case GDK_KEY_Escape:
			gtk_widget_hide(widget);
			return TRUE;

		case GDK_KEY_Tab:
			/* avoid leaving the entry */
			return TRUE;

		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_ISO_Enter:
			return TRUE;
	}

	return FALSE;
}

static void on_entry_text_notify(GObject *object, GParamSpec *pspec, gpointer dummy)
{
}

static void on_entry_activate(GtkEntry *entry, gpointer dummy)
{
}

static void on_prompt_hide(GtkWidget *widget, gpointer dummy)
{
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


static void clamp_cursor_pos(ScintillaObject *sci)
{
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
		plugin_data.default_caret_style = scintilla_send_message(sci, SCI_GETCARETSTYLE, 0, 0);

	if (plugin_data.vi_mode)
	{
		if (plugin_data.insert_mode)
			scintilla_send_message(sci, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
		else
			scintilla_send_message(sci, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
	}
	else
		scintilla_send_message(sci, SCI_SETCARETSTYLE, plugin_data.default_caret_style, 0);

	if (plugin_data.vi_mode)
	{
		const gchar *mode = plugin_data.insert_mode ? "INSERT" : "COMMAND";
		ui_set_statusbar(FALSE, "Vim Mode: -- %s --", mode);
	}

	if (!plugin_data.insert_mode)
		clamp_cursor_pos(sci);
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

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	ScintillaObject *sci;

	if (!doc)
		return FALSE;

	sci = doc->editor->sci;

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
				{
					const gchar *val = event->keyval == GDK_KEY_colon ? ":" : "/";
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
					sci_send_command(sci, SCI_CHARRIGHT);
					plugin_data.insert_mode = TRUE;
					prepare_vi_mode(doc);
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

	plugin_signal_connect(geany_plugin, G_OBJECT(doc->editor->sci), "key-press-event",
			FALSE, G_CALLBACK(on_key_press), NULL);
	prepare_vi_mode(doc);
}

static void on_doc_activate(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
		G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);

	prepare_vi_mode(doc);
}

PluginCallback plugin_callbacks[] = {
	{"document-open", (GCallback) &on_doc_open, TRUE, NULL},
	{"document-activate", (GCallback) &on_doc_activate, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};


void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *group;
	GtkWidget *frame;
	gsize i;

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
			//"default-height", 200,
			"transient-for", geany_data->main_widgets->window,
			"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
			"type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
			"skip-taskbar-hint", TRUE,
			"skip-pager-hint", TRUE,
			NULL);
	g_signal_connect(plugin_data.prompt, "focus-out-event", G_CALLBACK(gtk_widget_hide), NULL);
	g_signal_connect(plugin_data.prompt, "show", G_CALLBACK(on_prompt_show), NULL);
	g_signal_connect(plugin_data.prompt, "hide", G_CALLBACK(on_prompt_hide), NULL);
	g_signal_connect(plugin_data.prompt, "key-press-event", G_CALLBACK(on_prompt_key_press_event), NULL);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(plugin_data.prompt), frame);
  
	plugin_data.entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(frame), plugin_data.entry);

	g_signal_connect(plugin_data.entry, "notify::text", G_CALLBACK(on_entry_text_notify), NULL);
	g_signal_connect(plugin_data.entry, "activate", G_CALLBACK(on_entry_activate), NULL);

	gtk_widget_show_all(frame);

	/* final setup */
	foreach_document(i)
		plugin_signal_connect(geany_plugin, G_OBJECT(documents[i]->editor->sci),
				"key-press-event", FALSE, G_CALLBACK(on_key_press), NULL);
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
			scintilla_send_message(sci, SCI_SETCARETSTYLE, plugin_data.default_caret_style, 0);
		}
	}

	gtk_widget_destroy(plugin_data.prompt);
	gtk_widget_destroy(plugin_data.separator_item);
	gtk_widget_destroy(plugin_data.toggle_vi_item);
	gtk_widget_destroy(plugin_data.perform_vi_item);
}


void plugin_help(void)
{
	utils_open_browser("http://plugins.geany.org/geanyvim.html");
}
