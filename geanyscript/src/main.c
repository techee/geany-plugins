/*
 * Copyright 2017 Jiri Techet <techet@gmail.com>
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

#include <geanyplugin.h>

GeanyPlugin *geany_plugin;
GeanyData *geany_data;

PLUGIN_VERSION_CHECK(224)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("Geany Script"),
	_("TODO"),
	VERSION,
	"Jiri Techet <techet@gmail.com>")



PluginCallback plugin_callbacks[] = {
//	{"document-open", (GCallback) & on_doc_open, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};


static void reload(void);


static GSList *get_file_list(const gchar *locale_path, GHashTable *visited_paths)
{
	GSList *list = NULL;
	GDir *dir;
	gchar *real_path = tm_get_real_path(locale_path);

	dir = g_dir_open(locale_path, 0, NULL);
	if (!dir || !real_path || g_hash_table_lookup(visited_paths, real_path))
	{
		g_free(real_path);
		return NULL;
	}

	g_hash_table_insert(visited_paths, real_path, GINT_TO_POINTER(1));

	while (TRUE)
	{
		const gchar *locale_name;
		gchar *locale_filename;

		locale_name = g_dir_read_name(dir);
		if (!locale_name)
			break;

		locale_filename = g_build_filename(locale_path, locale_name, NULL);

		if (g_file_test(locale_filename, G_FILE_TEST_IS_DIR))
		{
			GSList *lst;

			lst = get_file_list(locale_filename, visited_paths);
			if (lst)
				list = g_slist_concat(list, lst);
		}
		else if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR) &&
				 g_str_has_suffix(locale_filename, ".conf"))
		{
			list = g_slist_prepend(list, g_strdup(locale_filename));
		}

		g_free(locale_filename);
	}

	g_dir_close(dir);

	return list;
}


typedef struct
{
	gchar *name;
	gchar *script_path;
} GeanyScript;


static void free_script(GeanyScript *script)
{
	g_free(script->name);
	script->name = NULL;
	g_free(script->script_path);
	script->script_path = NULL;
}

static GSList *g_scripts = NULL;
static GtkWidget *g_menu_item = NULL;

#define GEANYSCRIPT_KEY_FILE_GROUP "GeanyScript"
#define SCRIPT_DIR "/home/techet/scripts"


static GeanyScript *load_script(gchar *path)
{
	GeanyScript *script = NULL;
	GKeyFile *key_file = g_key_file_new();
	gchar *dirname = g_path_get_dirname(path);

	if (g_key_file_load_from_file(key_file, path, G_KEY_FILE_KEEP_COMMENTS, NULL))
	{
		gboolean success = TRUE;
		script = g_new0(GeanyScript, 1);

		script->name = g_key_file_get_string (key_file, GEANYSCRIPT_KEY_FILE_GROUP, "name", NULL);
		success = success && script->name;
		script->script_path = g_key_file_get_string (key_file, GEANYSCRIPT_KEY_FILE_GROUP, "script", NULL);
		success = success && script->script_path;
		if (success)
		{
			gchar *keyfile_name = utils_remove_ext_from_filename(path);
			gchar *script_name = utils_remove_ext_from_filename(script->script_path);

			SETPTR(keyfile_name, g_path_get_basename(keyfile_name));
			success = success && (g_strcmp0(keyfile_name, script_name) == 0);
			g_free(keyfile_name);
			g_free(script_name);
		}
		SETPTR(script->script_path, g_strconcat(dirname, G_DIR_SEPARATOR_S, script->script_path, NULL));
		success = success &&
			g_file_test(script->script_path, G_FILE_TEST_EXISTS) &&
			g_file_test(script->script_path, G_FILE_TEST_IS_EXECUTABLE);
		if (!success)
		{
			free_script(script);
			script = NULL;
		}
	}
	g_key_file_free(key_file);

	return script;
}


static gchar *get_relative_path(const gchar *locale_parent, const gchar *locale_descendant)
{
	GFile *gf_parent, *gf_descendant;
	gchar *locale_ret;

	gf_parent = g_file_new_for_path(locale_parent);
	gf_descendant = g_file_new_for_path(locale_descendant);

	locale_ret = g_file_get_relative_path(gf_parent, gf_descendant);

	g_object_unref(gf_parent);
	g_object_unref(gf_descendant);

	return locale_ret;
}


static GtkWidget *find_submenu(GtkWidget *menu, gchar *name)
{
	GtkWidget *submenu = NULL;
	GList *list, *node;

	list = gtk_container_get_children(GTK_CONTAINER(menu));

	foreach_list(node, list)
	{
		GtkMenuItem *item = node->data;
		const gchar *label;

		label = gtk_menu_item_get_label(item);
		if (g_strcmp0(label, name) == 0)
		{
			submenu = gtk_menu_item_get_submenu(item);
			if (submenu != NULL)
				break;
		}
	}

	g_list_free(list);

	return submenu;
}


static GtkWidget *create_item_with_submenu(const gchar *name)
{
	GtkWidget *item, *submenu;

	submenu = gtk_menu_new();
	gtk_widget_show(submenu);

	item = gtk_menu_item_new_with_mnemonic(name);
	gtk_widget_show(item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

	return item;
}


static gchar *get_keybinding_name(GeanyScript *script)
{
	gchar *path;

	path = get_relative_path(SCRIPT_DIR, script->script_path);
	SETPTR(path, g_path_get_dirname(path));

	if (g_strcmp0(path, ".") != 0)
	{
		gchar **comps = g_strsplit(path, G_DIR_SEPARATOR_S, -1);
		SETPTR(path, g_strjoinv("->", comps));
		SETPTR(path, g_strconcat(path, "->", script->name, NULL));
		g_strfreev(comps);
	}
	else
		SETPTR(path, g_strdup(script->name));

	return path;
}


static gboolean script_activated_cb(guint key_id)
{
	GSList *elem = g_slist_nth(g_scripts, key_id);
	GeanyScript *script;

	g_return_val_if_fail(elem != NULL, FALSE);

	script = elem->data;
	ui_set_statusbar(FALSE, "%s", script->script_path);

	return TRUE;
}


static void item_activated_cb(G_GNUC_UNUSED GtkMenuItem *menuitem, gpointer user_data)
{
	script_activated_cb(GPOINTER_TO_UINT(user_data));
}


static void create_entry(GeanyScript *script, GtkWidget *menu, gchar **dirs, guint i,
	GeanyKeyGroup *key_group)
{
	if (g_strv_length(dirs) == 0)
		return;
	else if (g_strv_length(dirs) == 1)
	{
		GtkWidget *item;
		gchar *label, *path;

		path = get_relative_path(SCRIPT_DIR, script->script_path);
		label = get_keybinding_name(script);

		item = gtk_menu_item_new_with_mnemonic(script->name);
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect(item, "activate", G_CALLBACK(item_activated_cb), GUINT_TO_POINTER(i));

		keybindings_set_item(key_group, i, NULL, 0, 0, path, label, item);
		g_free(label);
		g_free(path);
	}
	else
	{
		gchar *name;
		GtkWidget *submenu;

		name = utils_get_utf8_from_locale(dirs[0]);
		submenu = find_submenu(menu, name);
		if (!submenu)
		{
			GtkWidget *item;

			item = create_item_with_submenu(name);
			gtk_container_add(GTK_CONTAINER(menu), item);
			submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
		}
		create_entry(script, submenu, dirs + 1, i, key_group);
		g_free(name);
	}
}


static void create_menu_entry(GeanyScript *script, GtkWidget *menu, guint i, GeanyKeyGroup *key_group)
{
	gchar *rel_path;
	gchar **dirs;

	rel_path = get_relative_path(SCRIPT_DIR, script->script_path);
	dirs = g_strsplit(rel_path, G_DIR_SEPARATOR_S, -1);

	create_entry(script, menu, dirs, i, key_group);

	g_strfreev(dirs);
	g_free(rel_path);
}


static gint sort_scripts(GeanyScript *a, GeanyScript *b)
{
	gint res;
	gchar *a_dir, *b_dir;
	
	a_dir = g_path_get_dirname(a->script_path);
	b_dir = g_path_get_dirname(b->script_path);

	res = g_strcmp0(a_dir, b_dir);

	g_free(a_dir);
	g_free(b_dir);

	if (res != 0)
		return res;
	return g_strcmp0(a->name, b->name);
}


static void clean(void)
{
	GSList *node;

	foreach_slist(node, g_scripts)
		free_script((GeanyScript *)node->data);
	g_slist_free(g_scripts);
	g_scripts = NULL;

	if (g_menu_item)
	{
		gtk_widget_destroy(g_menu_item);
		g_menu_item = NULL;
	}
}


static void reload_cb(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	reload();
}


static void reload(void)
{
	GHashTable *visited_paths;
	GSList *files, *node;
	GtkWidget *item, *main_menu;
	GeanyKeyGroup *key_group;
	guint i = 0;

	clean();

	visited_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	files = get_file_list(SCRIPT_DIR, visited_paths);
	g_hash_table_destroy(visited_paths);

	foreach_slist(node, files)
	{
		GeanyScript *script = load_script((gchar *)node->data);
		if (script != NULL)
			g_scripts = g_slist_prepend(g_scripts, script);
	}

	g_scripts = g_slist_sort(g_scripts, (GCompareFunc)sort_scripts);

	g_menu_item = create_item_with_submenu("GeanyScript");
	main_menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(g_menu_item));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), g_menu_item);

	item = gtk_menu_item_new_with_mnemonic(_("Reload"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(main_menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(reload_cb), NULL);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(main_menu), item);

	key_group = plugin_set_key_group(geany_plugin,
		"GeanyScript", g_slist_length(g_scripts), script_activated_cb);

	foreach_slist(node, g_scripts)
	{
		GeanyScript *script = node->data;
		create_menu_entry(script, main_menu, i, key_group);
		i++;
	}

	keybindings_load_keyfile();

	g_slist_free(files);
}


void plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	reload();
}


void plugin_cleanup(void)
{
	clean();
}


void plugin_help (void)
{
	utils_open_browser("TODO");
}
