/*
 * Copyright 2024 Jiri Techet <techet@gmail.com>
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
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "prjorg-file-monitor.h"
#include "prjorg-project.h"
#include "prjorg-sidebar.h"

#include <gio/gio.h>

typedef struct
{
	gchar *name;
	gchar *dirname;
	GPtrArray *subtrees;
	GFileMonitor *monitor;
} MonitorTree;

static MonitorTree *get_monitor_for_path(const gchar *root, const gchar *dirname,
	gboolean create, gboolean delete);


static GPtrArray *roots;


static void free_monitor(MonitorTree *tree)
{
	MonitorTree *subtree;
	guint i;

	g_object_unref(tree->monitor);
	g_free(tree->dirname);
	g_free(tree->name);
	foreach_ptr_array(subtree, i, tree->subtrees)
	{
		free_monitor(subtree);
	}
}


static const gchar *get_root_path(GFile *path)
{
	GSList *elem;

	if (!prj_org)
		return NULL;

	foreach_slist(elem, prj_org->roots)
	{
		PrjOrgRoot *root = elem->data;
		gchar *locale_parent = utils_get_locale_from_utf8(root->base_dir);
		GFile *gf_parent = g_file_new_for_path(locale_parent);
		gchar *relative = g_file_get_relative_path(gf_parent, path);
		gboolean is_subdir = !g_str_has_prefix(relative, "..");

		g_object_unref(gf_parent);
		g_free(locale_parent);

		if (is_subdir)
			return root->base_dir;
	}

	return FALSE;
}


static void on_changed(GFileMonitor *self, GFile *file, GFile *other_file,
	GFileMonitorEvent event_type, gpointer user_data)
{
	GFileInfo *info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
		G_FILE_QUERY_INFO_NONE, NULL, NULL);
	gchar *root_locale = utils_get_locale_from_utf8(get_root_path(file));
	gchar *fname_locale = g_file_get_path(file);
	gchar *fname_utf8 = utils_get_utf8_from_locale(fname_locale);;
	gboolean is_directory = FALSE;
	gboolean have_is_directory = TRUE;

	if (info)
		is_directory = g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY;
	else if (event_type == G_FILE_MONITOR_EVENT_DELETED)
	{
		/* For delete events the file/directory is gone so we don't have valid
		 * info. However, we monitor all directories so when we have a monitor,
		 * file is a directory, otherwise it's a file */
		is_directory = get_monitor_for_path(root_locale, fname_locale, FALSE, FALSE) != NULL;
	}
	else
		have_is_directory = FALSE;

	if (have_is_directory && root_locale && fname_locale && fname_utf8)
	{
		if (is_directory)
		{
			if (event_type == G_FILE_MONITOR_EVENT_DELETED)
			{
				printf("deleting monitor for %s\n", fname_locale);
				get_monitor_for_path(root_locale, fname_locale, FALSE, TRUE);
			}
			else if (event_type == G_FILE_MONITOR_EVENT_CREATED)
			{
				printf("installing monitor for %s\n", fname_locale);
				get_monitor_for_path(root_locale, fname_locale, TRUE, FALSE);
			}
		}
		else
		{
			if (event_type == G_FILE_MONITOR_EVENT_DELETED)
			{
				printf("deleted %s\n", fname_utf8);
				prjorg_sidebar_remove_file(fname_utf8);
				prjorg_project_remove_file(fname_utf8);
			}
			else if (event_type == G_FILE_MONITOR_EVENT_CREATED)
			{
				printf("created %s\n", fname_utf8);
				prjorg_project_add_file(fname_utf8);
				prjorg_sidebar_add_file(fname_utf8);
			}
		}
	}
	else
	{
		printf("giving up %s\n", fname_locale);
	}

	g_free(fname_locale);
	g_free(fname_utf8);
	g_free(root_locale);
	if (info)
		g_object_unref(info);
}


static MonitorTree *create_monitor(const gchar *name, const gchar *dirname)
{
	GFile *dir = g_file_new_for_path(dirname);
	MonitorTree *tree;

	tree = g_new0(MonitorTree, 1);
	tree->name = g_strdup(name);
	tree->dirname = g_strdup(dirname);
	tree->subtrees = g_ptr_array_new_full(1, (GDestroyNotify)free_monitor);
	tree->monitor = g_file_monitor_directory(dir, 0, NULL, NULL);
	g_signal_connect(tree->monitor, "changed", G_CALLBACK(on_changed), NULL);

	g_object_unref(dir);

	return tree;
}


static MonitorTree *get_monitor(const gchar *dirname, gchar **dirv, guint dirv_len, guint dirv_pos,
	GPtrArray *trees, gboolean create, gboolean delete)
{
	const gchar *current_dir = dirv[dirv_pos];
	MonitorTree *tree;
	guint i;

	foreach_ptr_array(tree, i, trees)
	{
		if (g_strcmp0(tree->name, current_dir) == 0)
		{
			if (dirv_pos + 1 == dirv_len)
			{
				if (delete)
				{
					g_ptr_array_remove_fast(trees, tree);
					return NULL;
				}

				return tree;
			}

			return get_monitor(dirname, dirv, dirv_len, dirv_pos + 1, tree->subtrees, create, delete);
		}
	}

	if (!create)
		return NULL;

	tree = create_monitor(current_dir, dirname);
	g_ptr_array_add(trees, tree);

	if (dirv_pos + 1 == dirv_len)
		return tree;

	return get_monitor(dirname, dirv, dirv_len, dirv_pos + 1, tree->subtrees, create, delete);
}


static MonitorTree *get_monitor_for_path(const gchar *root, const gchar *dirname,
	gboolean create, gboolean delete)
{
	gchar **rootv = g_strsplit(root, G_DIR_SEPARATOR_S, -1);
	gchar **dirv = g_strsplit(dirname, G_DIR_SEPARATOR_S, -1);
	guint rootv_len = g_strv_length(rootv);
	guint dirv_len = g_strv_length(dirv);
	guint pos = rootv_len - 1;
	MonitorTree *monitor;

	if (!roots)
		roots = g_ptr_array_new_full(1, (GDestroyNotify)free_monitor);

	monitor = get_monitor(dirname, dirv, dirv_len, pos, roots, create, delete);

	g_strfreev(rootv);
	g_strfreev(dirv);

	return monitor;
}


void prjorg_file_monitor_add_directory(const gchar *root, const gchar *dirname)
{
	get_monitor_for_path(root, dirname, TRUE, FALSE);
}


static void print_indent(gint indentation)
{
	for (guint i = 0; i < indentation; i++)
		printf(" ");
}


static void print_hierarchy(GPtrArray *subtrees, gint indentation)
{
	MonitorTree *tree;
	guint i;

	foreach_ptr_array(tree, i, subtrees)
	{
		print_indent(indentation);
		printf("%s\n", tree->dirname);
		if (tree->subtrees->len > 0)
			print_hierarchy(tree->subtrees, indentation + 2);
	}
}


void prjorg_file_monitor_print_hierarchy(void)
{
	print_hierarchy(roots, 0);
}
