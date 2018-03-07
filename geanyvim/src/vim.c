/*
 * Copyright (C) 2018 Jiri Techet <techet@gmail.com>
 * Copyright (C) 2012 Colomban Wendling <ban@herbesfolles.org>
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

	gboolean vi_mode;
	gboolean onetime_vi_mode;
} plugin_data =
{
	NULL, NULL, NULL, NULL, NULL, TRUE, FALSE
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



static void create_prompt(void)
{
	GtkWidget *frame;
	GtkWidget *box;

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
  
	box = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), box);

	plugin_data.entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(box), plugin_data.entry, FALSE, TRUE, 0);

	/* connect entry signals after the view is created as they use it */
	g_signal_connect(plugin_data.entry, "notify::text", G_CALLBACK(on_entry_text_notify), NULL);
	g_signal_connect(plugin_data.entry, "activate", G_CALLBACK(on_entry_activate), NULL);

	gtk_widget_show_all(frame);
}


static gchar *get_config_filename (void)
{
	return g_build_filename (geany_data->app->configdir, "plugins", PLUGIN, PLUGIN".conf", NULL);
}

static void load_config(void)
{
	gchar *filename = get_config_filename ();
	GKeyFile *kf = g_key_file_new ();

	if (g_key_file_load_from_file (kf, filename, G_KEY_FILE_NONE, NULL))
		plugin_data.vi_mode = g_key_file_get_boolean (kf, CONF_GROUP, CONF_VI_MODE, NULL);
  
	g_key_file_free (kf);
	g_free (filename);
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


static void enable_vi_mode()
{
	
}

static void disable_vi_mode()
{
	
}



static gboolean on_toggle_vim_mode(GeanyKeyBinding *kb, guint key_id, gpointer data)
{
	plugin_data.vi_mode = !plugin_data.vi_mode;
	plugin_data.onetime_vi_mode = FALSE;
	if (plugin_data.vi_mode)
		enable_vi_mode();
	else
		disable_vi_mode();
	save_config();

	return TRUE;
}

static gboolean on_perform_vim_command(GeanyKeyBinding *kb, guint key_id, gpointer data)
{
	if (!plugin_data.vi_mode)
	{
		plugin_data.onetime_vi_mode = TRUE;
		plugin_data.vi_mode = TRUE;
		enable_vi_mode();
	}
	return TRUE;
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	if (plugin_data.vi_mode)
	{
		if (event->keyval == GDK_KEY_colon || event->keyval == GDK_KEY_slash)
		{
			gtk_widget_show(plugin_data.prompt);
		}
		return TRUE;
	}
	printf("now\n");
	return FALSE;
}


static void on_doc_open(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
		G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);

	plugin_signal_connect(geany_plugin, G_OBJECT(doc->editor->sci), "key-press-event",
			FALSE, G_CALLBACK(on_key_press), NULL);
}


PluginCallback plugin_callbacks[] = {
	{"document-open", (GCallback) &on_doc_open, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};


void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *group;
	gsize i;

	load_config();

	foreach_document(i)
		plugin_signal_connect(geany_plugin, G_OBJECT(documents[i]->editor->sci),
				"key-press-event", FALSE, G_CALLBACK(on_key_press), NULL);

	group = plugin_set_key_group(geany_plugin, "geanyvim", KB_COUNT, NULL);

	plugin_data.separator_item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), plugin_data.separator_item);

	plugin_data.toggle_vi_item = gtk_check_menu_item_new_with_mnemonic(_("Enable _Vim Mode"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(plugin_data.toggle_vi_item),
			plugin_data.vi_mode);
	gtk_widget_show(plugin_data.toggle_vi_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), plugin_data.toggle_vi_item);
	g_signal_connect((gpointer) plugin_data.toggle_vi_item, "activate", G_CALLBACK(on_toggle_vim_mode), NULL);
	keybindings_set_item_full(group, KB_TOGGLE_VIM_MODE, 0, 0, "toggle_vim",
			_("Enable Vim Mode"), NULL, on_toggle_vim_mode, NULL, NULL);

	plugin_data.perform_vi_item = gtk_menu_item_new_with_mnemonic(_("_Perform Vim Command"));
	gtk_widget_show(plugin_data.perform_vi_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), plugin_data.perform_vi_item);
	g_signal_connect((gpointer) plugin_data.perform_vi_item, "activate", G_CALLBACK(on_perform_vim_command), NULL);
	keybindings_set_item_full(group, KB_PERFORM_VIM_COMMAND, 0, 0, "vim_command",
			_("Perform Vim Command"), NULL, on_perform_vim_command, NULL, NULL);

	create_prompt();
}


void plugin_cleanup(void)
{
	gtk_widget_destroy(plugin_data.prompt);
	gtk_widget_destroy(plugin_data.separator_item);
	gtk_widget_destroy(plugin_data.toggle_vi_item);
	gtk_widget_destroy(plugin_data.perform_vi_item);
}


void plugin_help(void)
{
	utils_open_browser("http://plugins.geany.org/geanyvim.html");
}
