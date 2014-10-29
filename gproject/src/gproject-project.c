/*
 * Copyright 2010 Jiri Techet <techet@gmail.com>
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

#include <sys/time.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gstdio.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "gproject-utils.h"
#include "gproject-project.h"

extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;
extern GeanyFunctions *geany_functions;

typedef struct
{
	GtkWidget *source_patterns;
	GtkWidget *header_patterns;
	GtkWidget *ignored_dirs_patterns;
	GtkWidget *generate_tag_prefs;
} PropertyDialogElements;

GPrj *g_prj = NULL;

static PropertyDialogElements *e;

static GSList *s_idle_add_funcs;
static GSList *s_idle_remove_funcs;


static void clear_idle_queue(GSList **queue)
{
	g_slist_free_full(*queue, g_free);
	*queue = NULL;
}


static void workspace_remove_tm_source_file_cb(gchar *filename, TMSourceFile *sf, gpointer foo)
{
	if (sf != NULL)
	{
		tm_workspace_remove_source_file(sf, FALSE);
		tm_source_file_free(sf);
	}
}


/* path - absolute path in locale, returned list in locale */
static GSList *get_file_list(const gchar * path, GSList *patterns, GSList *ignored_dirs_patterns)
{
	GSList *list = NULL;
	GDir *dir;

	dir = g_dir_open(path, 0, NULL);
	if (!dir)
		return NULL;

	while (TRUE)
	{
		const gchar *name;
		gchar *filename;

		name = g_dir_read_name(dir);
		if (!name)
			break;

		filename = g_build_filename(path, name, NULL);

		if (g_file_test(filename, G_FILE_TEST_IS_DIR))
		{
			GSList *lst;

			if (patterns_match(ignored_dirs_patterns, name))
			{
				g_free(filename);
				continue;
			}

			lst = get_file_list(filename, patterns, ignored_dirs_patterns);
			if (lst)
				list = g_slist_concat(list, lst);
			g_free(filename);
		}
		else if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
		{
			if (patterns_match(patterns, name))
				list = g_slist_prepend(list, filename);
			else
				g_free(filename);
		}
	}

	g_dir_close(dir);

	return list;
}


static gint gprj_project_rescan_root(GPrjRoot *root)
{
	GSList *pattern_list = NULL;
	GSList *ignored_dirs_list = NULL;
	GSList *lst;
	GSList *elem;
	gint filenum = 0;

	g_hash_table_foreach(root->file_table, (GHFunc)workspace_remove_tm_source_file_cb, NULL);
	g_hash_table_remove_all(root->file_table);

	if (!geany_data->app->project->file_patterns || !geany_data->app->project->file_patterns[0])
	{
		gchar **all_pattern = g_strsplit ("*", " ", -1);
		pattern_list = get_precompiled_patterns(all_pattern);
		g_strfreev(all_pattern);
	}
	else
		pattern_list = get_precompiled_patterns(geany_data->app->project->file_patterns);

	ignored_dirs_list = get_precompiled_patterns(g_prj->ignored_dirs_patterns);

	lst = get_file_list(root->base_dir, pattern_list, ignored_dirs_list);
	
	foreach_slist(elem, lst)
	{
		char *path;

		path = tm_get_real_path(elem->data);
		if (path)
		{
			SETPTR(path, utils_get_utf8_from_locale(path));
			g_hash_table_insert(root->file_table, path, NULL);
			filenum++;
		}
	}

	g_slist_foreach(lst, (GFunc) g_free, NULL);
	g_slist_free(lst);

	g_slist_foreach(pattern_list, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(pattern_list);

	g_slist_foreach(ignored_dirs_list, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(ignored_dirs_list);
	
	return filenum;
}


static void regenerate_tags(GPrjRoot *root, gpointer user_data)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init(&iter, root->file_table);
	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		TMSourceFile *sf;
		gchar *path = key;
		
		sf = tm_source_file_new(path, filetypes_detect_from_file(path)->name);
		if (sf && !document_find_by_filename(path))
		{
			tm_workspace_add_source_file(sf);
			tm_workspace_update_source_file(sf, FALSE);
		}
		
		g_hash_table_iter_replace(&iter, sf);
	}
}


void gprj_project_rescan(void)
{
	GSList *elem;
	gint filenum = 0;
	
	if (!g_prj)
		return;

	clear_idle_queue(&s_idle_add_funcs);
	clear_idle_queue(&s_idle_remove_funcs);
	
	foreach_slist(elem, g_prj->roots)
		filenum += gprj_project_rescan_root(elem->data);
	
	if (g_prj->generate_tag_prefs == GPrjTagYes || (g_prj->generate_tag_prefs == GPrjTagAuto && filenum < 5000))
		g_slist_foreach(g_prj->roots, (GFunc)regenerate_tags, NULL);

	tm_workspace_update();
}


static void update_project(
	gchar **source_patterns,
	gchar **header_patterns,
	gchar **ignored_dirs_patterns,
	GPrjTagPrefs generate_tag_prefs)
{
	if (g_prj->source_patterns)
		g_strfreev(g_prj->source_patterns);
	g_prj->source_patterns = g_strdupv(source_patterns);

	if (g_prj->header_patterns)
		g_strfreev(g_prj->header_patterns);
	g_prj->header_patterns = g_strdupv(header_patterns);

	if (g_prj->ignored_dirs_patterns)
		g_strfreev(g_prj->ignored_dirs_patterns);
	g_prj->ignored_dirs_patterns = g_strdupv(ignored_dirs_patterns);

	g_prj->generate_tag_prefs = generate_tag_prefs;

	gprj_project_rescan();
}


void gprj_project_save(GKeyFile * key_file)
{
	GPtrArray *array;
	GSList *elem, *lst;
	
	if (!g_prj)
		return;

	g_key_file_set_string_list(key_file, "gproject", "source_patterns",
		(const gchar**) g_prj->source_patterns, g_strv_length(g_prj->source_patterns));
	g_key_file_set_string_list(key_file, "gproject", "header_patterns",
		(const gchar**) g_prj->header_patterns, g_strv_length(g_prj->header_patterns));
	g_key_file_set_string_list(key_file, "gproject", "ignored_dirs_patterns",
		(const gchar**) g_prj->ignored_dirs_patterns, g_strv_length(g_prj->ignored_dirs_patterns));
	g_key_file_set_integer(key_file, "gproject", "generate_tag_prefs", g_prj->generate_tag_prefs);
	
	array = g_ptr_array_new();
	lst = g_prj->roots->next;
	foreach_slist (elem, lst)
	{
		GPrjRoot *root = elem->data;
		g_ptr_array_add(array, root->base_dir);
	}
	g_key_file_set_string_list(key_file, "gproject", "external_dirs", (const gchar * const *)array->pdata, array->len);
	g_ptr_array_free(array, TRUE);
}


static GPrjRoot *create_root(const gchar *base_dir)
{
	GPrjRoot *root = (GPrjRoot *) g_new0(GPrjRoot, 1);
	root->base_dir = g_strdup(base_dir);
	root->file_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	return root;
}


static void close_root(GPrjRoot *root, gpointer user_data)
{
	g_hash_table_foreach(root->file_table, (GHFunc)workspace_remove_tm_source_file_cb, NULL);
	g_hash_table_destroy(root->file_table);
	g_free(root->base_dir);
	g_free(root);
}


static gint root_comparator(GPrjRoot *a, GPrjRoot *b)
{
	return g_strcmp0(a->base_dir, b->base_dir);
}


void gprj_project_add_external_dir(const gchar *dirname)
{
	GPrjRoot *new_root = create_root(dirname);
	if (g_slist_find_custom (g_prj->roots, new_root, (GCompareFunc)root_comparator) != NULL)
	{
		close_root(new_root, NULL);
		return;
	}
	
	GSList *lst = g_prj->roots->next;
	lst = g_slist_prepend(lst, new_root);
	lst = g_slist_sort(lst, (GCompareFunc)root_comparator);
	g_prj->roots->next = lst;
	
	gprj_project_rescan();
}


void gprj_project_remove_external_dir(const gchar *dirname)
{
	GPrjRoot *test_root = create_root(dirname);
	GSList *found = g_slist_find_custom (g_prj->roots, test_root, (GCompareFunc)root_comparator);
	if (found != NULL)
	{
		GPrjRoot *found_root = found->data;
		
		g_prj->roots = g_slist_remove(g_prj->roots, found_root);
		close_root(found_root, NULL);
		gprj_project_rescan();
	}
	close_root(test_root, NULL);
}


void gprj_project_open(GKeyFile * key_file)
{
	gchar **source_patterns, **header_patterns, **ignored_dirs_patterns, **external_dirs, **dir_ptr, *last_name;
	gint generate_tag_prefs;
	GSList *elem, *ext_list = NULL;

	if (g_prj != NULL)
		gprj_project_close();

	g_prj = (GPrj *) g_new0(GPrj, 1);

	g_prj->source_patterns = NULL;
	g_prj->header_patterns = NULL;
	g_prj->ignored_dirs_patterns = NULL;
	g_prj->generate_tag_prefs = GPrjTagAuto;

	source_patterns = g_key_file_get_string_list(key_file, "gproject", "source_patterns", NULL, NULL);
	if (!source_patterns)
		source_patterns = g_strsplit("*.c *.C *.cpp *.cxx *.c++ *.cc *.m", " ", -1);
	header_patterns = g_key_file_get_string_list(key_file, "gproject", "header_patterns", NULL, NULL);
	if (!header_patterns)
		header_patterns = g_strsplit("*.h *.H *.hpp *.hxx *.h++ *.hh", " ", -1);
	ignored_dirs_patterns = g_key_file_get_string_list(key_file, "gproject", "ignored_dirs_patterns", NULL, NULL);
	if (!ignored_dirs_patterns)
		ignored_dirs_patterns = g_strsplit(".* CVS", " ", -1);
	generate_tag_prefs = utils_get_setting_integer(key_file, "gproject", "generate_tag_prefs", GPrjTagAuto);

	external_dirs = g_key_file_get_string_list(key_file, "gproject", "external_dirs", NULL, NULL);
	foreach_strv (dir_ptr, external_dirs)
		ext_list = g_slist_prepend(ext_list, *dir_ptr);
	ext_list = g_slist_sort(ext_list, (GCompareFunc)g_strcmp0);
	last_name = NULL;
	foreach_slist (elem, ext_list)
	{
		if (g_strcmp0(last_name, elem->data) != 0)
			g_prj->roots = g_slist_append(g_prj->roots, create_root(elem->data));
		last_name = elem->data;
	}
	g_slist_free(ext_list);
	/* the project directory is always first */
	g_prj->roots = g_slist_prepend(g_prj->roots, create_root(geany_data->app->project->base_path));

	update_project(
		source_patterns,
		header_patterns,
		ignored_dirs_patterns,
		generate_tag_prefs);

	g_strfreev(source_patterns);
	g_strfreev(header_patterns);
	g_strfreev(ignored_dirs_patterns);
	g_strfreev(external_dirs);
}


static gchar **split_patterns(const gchar *str)
{
	GString *tmp;
	gchar **ret;
	gchar *input;

	input = g_strdup(str);

	g_strstrip(input);
	tmp = g_string_new(input);
	g_free(input);
	do {} while (utils_string_replace_all(tmp, "  ", " "));
	ret = g_strsplit(tmp->str, " ", -1);
	g_string_free(tmp, TRUE);
	return ret;
}


void gprj_project_read_properties_tab(void)
{
	gchar **source_patterns, **header_patterns, **ignored_dirs_patterns;

	source_patterns = split_patterns(gtk_entry_get_text(GTK_ENTRY(e->source_patterns)));
	header_patterns = split_patterns(gtk_entry_get_text(GTK_ENTRY(e->header_patterns)));
	ignored_dirs_patterns = split_patterns(gtk_entry_get_text(GTK_ENTRY(e->ignored_dirs_patterns)));

	update_project(
		source_patterns, header_patterns, ignored_dirs_patterns,
		gtk_combo_box_get_active(GTK_COMBO_BOX(e->generate_tag_prefs)));

	g_strfreev(source_patterns);
	g_strfreev(header_patterns);
	g_strfreev(ignored_dirs_patterns);
}


gint gprj_project_add_properties_tab(GtkWidget *notebook)
{
	GtkWidget *vbox, *hbox, *hbox1;
	GtkWidget *table;
	GtkWidget *label;
	gchar *str;
	gint page_index;

	e = g_new0(PropertyDialogElements, 1);

	vbox = gtk_vbox_new(FALSE, 0);

	table = gtk_table_new(4, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	label = gtk_label_new(_("Source patterns:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->source_patterns = gtk_entry_new();
	ui_table_add_row(GTK_TABLE(table), 0, label, e->source_patterns, NULL);
	ui_entry_add_clear_icon(GTK_ENTRY(e->source_patterns));
	ui_widget_set_tooltip_text(e->source_patterns,
		_("Space separated list of patterns that are used to identify source files. "
		  "Used for header/source swapping."));
	str = g_strjoinv(" ", g_prj->source_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->source_patterns), str);
	g_free(str);

	label = gtk_label_new(_("Header patterns:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->header_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->header_patterns));
	ui_table_add_row(GTK_TABLE(table), 1, label, e->header_patterns, NULL);
	ui_widget_set_tooltip_text(e->header_patterns,
		_("Space separated list of patterns that are used to identify headers. "
		  "Used for header/source swapping."));
	str = g_strjoinv(" ", g_prj->header_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->header_patterns), str);
	g_free(str);

	label = gtk_label_new(_("Ignored directory patterns:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->ignored_dirs_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->ignored_dirs_patterns));
	ui_table_add_row(GTK_TABLE(table), 2, label, e->ignored_dirs_patterns, NULL);
	ui_widget_set_tooltip_text(e->ignored_dirs_patterns,
		_("Space separated list of patterns that are used to identify directories "
		  "that are not scanned for source files."));
	str = g_strjoinv(" ", g_prj->ignored_dirs_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->ignored_dirs_patterns), str);
	g_free(str);

	label = gtk_label_new(_("Generate tags for all project files:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->generate_tag_prefs = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e->generate_tag_prefs), _("Auto (generate if less than 5000 files)"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e->generate_tag_prefs), _("Yes"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e->generate_tag_prefs), _("No"));
	gtk_combo_box_set_active(GTK_COMBO_BOX_TEXT(e->generate_tag_prefs), g_prj->generate_tag_prefs);
	ui_table_add_row(GTK_TABLE(table), 3, label, e->generate_tag_prefs, NULL);
	ui_widget_set_tooltip_text(e->generate_tag_prefs,
		_("Generate tag list for all project files instead of only for the currently opened files. "
		  "Might be slow for big projects."));

	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 6);

	hbox1 = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Note: set the patterns of files belonging to the project under the Project tab."));
	gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 6);

	label = gtk_label_new("GProject");

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 6);

	page_index = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hbox, label);
	gtk_widget_show_all(notebook);

	return page_index;
}


void gprj_project_close(void)
{
	if (!g_prj)
		return;  /* can happen on plugin reload */

	clear_idle_queue(&s_idle_add_funcs);
	clear_idle_queue(&s_idle_remove_funcs);

	g_slist_foreach(g_prj->roots, (GFunc)close_root, NULL);
	g_slist_free(g_prj->roots);
	tm_workspace_update();

	g_strfreev(g_prj->source_patterns);
	g_strfreev(g_prj->header_patterns);
	g_strfreev(g_prj->ignored_dirs_patterns);

	g_free(g_prj);
	g_prj = NULL;
}


gboolean gprj_project_is_in_project(const gchar * filename)
{
	GSList *elem;
	
	if (!filename || !g_prj || !geany_data->app->project || !g_prj->roots)
		return FALSE;
	
	foreach_slist (elem, g_prj->roots)
	{
		GPrjRoot *root = elem->data;
		if (g_hash_table_contains(root->file_table, filename))
			return TRUE;
	}

	return FALSE;
}


static gboolean add_tm_idle(gpointer foo)
{
	GSList *elem2;
	
	if (!g_prj || !s_idle_add_funcs)
		return FALSE;

	foreach_slist (elem2, s_idle_add_funcs)
	{
		GSList *elem;
		gchar *fname = elem2->data;
		
		foreach_slist (elem, g_prj->roots)
		{
			GPrjRoot *root = elem->data;
			TMSourceFile *sf = g_hash_table_lookup(root->file_table, fname);
			
			if (sf != NULL && !document_find_by_filename(fname))
			{
				tm_workspace_add_source_file(sf);
				tm_workspace_update_source_file(sf, TRUE);
				break;  /* single file representation in TM is enough */
			}
		}
	}
	
	clear_idle_queue(&s_idle_add_funcs);

	return FALSE;
}


/* This function gets called when document is being closed by Geany and we need
 * to add the TMSourceFile from the tag manager because Geany removes it on
 * document close.
 * 
 * Additional problem: The tag removal in Geany happens after this function is called. 
 * To be sure, perform on idle after this happens (even though from my knowledge of TM
 * this shouldn't probably matter). */
void gprj_project_add_single_tm_file(gchar *filename)
{
	if (s_idle_add_funcs == NULL)
		plugin_idle_add(geany_plugin, (GSourceFunc)add_tm_idle, NULL);
	
	s_idle_add_funcs = g_slist_prepend(s_idle_add_funcs, g_strdup(filename));
}


static gboolean remove_tm_idle(gpointer foo)
{
	GSList *elem2;
	
	if (!g_prj || !s_idle_remove_funcs)
		return FALSE;

	foreach_slist (elem2, s_idle_remove_funcs)
	{
		GSList *elem;
		gchar *fname = elem2->data;

		foreach_slist (elem, g_prj->roots)
		{
			GPrjRoot *root = elem->data;
			TMSourceFile *sf = g_hash_table_lookup(root->file_table, fname);
			
			if (sf != NULL)
				tm_workspace_remove_source_file(sf, TRUE);
		}
	}
	
	clear_idle_queue(&s_idle_remove_funcs);

	return FALSE;
}


/* This function gets called when document is being opened by Geany and we need
 * to remove the TMSourceFile from the tag manager because Geany inserts
 * it for the newly open tab. Even though tag manager would handle two identical
 * files, the file inserted by the plugin isn't updated automatically in TM
 * so any changes wouldn't be reflected in the tags array (e.g. removed function
 * from source file would still be found in TM)
 * 
 * Additional problem: The document being opened may be caused
 * by going to tag definition/declaration - tag processing is in progress
 * when this function is called and if we remove the TmSourceFile now, line
 * number for the searched tag won't be found. For this reason delay the tag
 * TmSourceFile removal until idle */
void gprj_project_remove_single_tm_file(gchar *filename)
{
	if (s_idle_remove_funcs == NULL)
		plugin_idle_add(geany_plugin, (GSourceFunc)remove_tm_idle, NULL);
	
	s_idle_remove_funcs = g_slist_prepend(s_idle_remove_funcs, g_strdup(filename));
}
