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


void plugin_init(G_GNUC_UNUSED GeanyData * data)
{
}


void plugin_cleanup(void)
{
}


void plugin_help (void)
{
	utils_open_browser("TODO");
}
