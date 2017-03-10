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


void plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	GHashTable *visited_paths;
	GSList *files, *node;
	
	visited_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	files = get_file_list("/home/techet/scripts", visited_paths);
	g_hash_table_destroy(visited_paths);

	foreach_slist(node, files)
	{
		printf("%s\n", (char *)node->data);
	}

	
	g_slist_free(files);
}


void plugin_cleanup(void)
{
}


void plugin_help (void)
{
	utils_open_browser("TODO");
}
