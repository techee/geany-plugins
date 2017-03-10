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

GSList *g_scripts = NULL;

#define GEANYSCRIPT_KEY_FILE_GROUP "GeanyScript"


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


void plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	GHashTable *visited_paths;
	GSList *files, *node;
	
	visited_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	files = get_file_list("/home/techet/scripts", visited_paths);
	g_hash_table_destroy(visited_paths);

	foreach_slist(node, files)
	{
		GeanyScript *script = load_script((gchar *)node->data);
		if (script != NULL)
		{
			g_scripts = g_slist_prepend(g_scripts, script);
			printf("%s   %s\n", script->name, script->script_path);
		}
	}

	g_slist_free(files);
}


void plugin_cleanup(void)
{
	GSList *node;
	foreach_slist(node, g_scripts)
		free_script((GeanyScript *)node->data);
}


void plugin_help (void)
{
	utils_open_browser("TODO");
}
