/*
 *	  Copyright 2010-2014 Jiri Techet <techet@gmail.com>
 *
 *	  This program is free software; you can redistribute it and/or modify
 *	  it under the terms of the GNU General Public License as published by
 *	  the Free Software Foundation; either version 2 of the License, or
 *	  (at your option) any later version.
 *
 *	  This program is distributed in the hope that it will be useful,
 *	  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	  GNU General Public License for more details.
 *
 *	  You should have received a copy of the GNU General Public License
 *	  along with this program; if not, write to the Free Software
 *	  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <sys/time.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>

#include "readtags.h"

#include <errno.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>


PLUGIN_VERSION_CHECK(211)
PLUGIN_SET_INFO("Goimports",
	_("Run goimports on save"),
	VERSION,
	"Jiri Techet <techet@gmail.com>")

GeanyPlugin *geany_plugin;
GeanyData *geany_data;
GeanyFunctions *geany_functions;


static void spawn_cmd(const gchar *cmd)
{
	GError *error = NULL;
	gchar **argv;
	gchar *utf8_cmd_string;
	gchar *out;
	gint exitcode;
	gboolean success;

	argv = g_new0(gchar *, 4);
	argv[0] = g_strdup("/bin/sh");
	argv[1] = g_strdup("-c");
	argv[2] = g_strdup(cmd);
	argv[3] = NULL;

	success = utils_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
			NULL, NULL, NULL, NULL, &exitcode, &error);

	g_strfreev(argv);
}


static void on_document_before_save(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (g_str_has_suffix(doc->file_name, ".go"))
	{
		gchar *cont, *fname, *cmd;
		gint fd, old_pos, old_line, old_len, old_col, old_lines, new_len, new_lines;
		ScintillaObject	*sci = doc->editor->sci;

		old_pos = sci_get_current_position(sci);
		old_line = sci_get_current_line(sci);
		old_col = sci_get_col_from_position(sci, old_pos);
		old_len = sci_get_length(sci);
		old_lines = sci_get_line_count(sci);

		cont = sci_get_contents(sci, -1);
		fd = g_file_open_tmp("geany_goimports_XXXXXX", &fname, NULL);
		close(fd);
		g_file_set_contents(fname, cont, -1, NULL);
		g_free(cont);

		cmd = g_strconcat("goimports -w ", fname, NULL);
		spawn_cmd(cmd);
		g_free(cmd);

		g_file_get_contents(fname, &cont, NULL, NULL);
		sci_set_text(sci, cont);
		g_free(cont);

		new_len = sci_get_length(sci);
		new_lines = sci_get_line_count(sci);

		gint pos = old_pos + (new_len - old_len);
		if (pos < old_pos)
		{
			gint line;
			if (pos < 0)
				pos = 0;
			
			while (sci_get_line_from_position(sci, pos) < old_line + (new_lines - old_lines) && pos < new_len)
				pos++;
			while (sci_get_col_from_position(sci, pos) > old_col && pos < new_len)
				pos++;

//			gint line_len = sci_get_line_length(sci, sci_get_line_from_position(sci, pos));
//			pos += line_len >= old_pos ? old_pos : line_len;
		}
		else if (pos > old_pos) {
			if (pos >= new_len)
				pos = new_len - 1;

			while (sci_get_line_from_position(sci, pos) > old_line + (new_lines - old_lines) && pos > 0)
				pos--;
			while (sci_get_col_from_position(sci, pos) < old_col && pos > 0)
				pos--;

//			gint line_len = sci_get_line_length(sci, sci_get_line_from_position(sci, pos));
//			pos -= line_len;
//			pos += line_len >= old_pos ? old_pos : line_len;
		}

		sci_set_current_position(sci, pos, FALSE);

		g_unlink(fname);
		g_free(fname);
	}
}


PluginCallback plugin_callbacks[] = {
	{"document-before-save", (GCallback) & on_document_before_save, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};


void plugin_help (void)
{
	utils_open_browser (DOCDIR "/" PLUGIN "/README");
}


void plugin_init(G_GNUC_UNUSED GeanyData * data)
{
}

void plugin_cleanup(void)
{
}
