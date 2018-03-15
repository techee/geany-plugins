/*
 * Copyright 2018 Jiri Techet <techet@gmail.com>
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

#ifndef __GEANYVIM_CMDS_H__
#define __GEANYVIM_CMDS_H__

#include <glib.h>
#include <geanyplugin.h>


typedef struct
{
	guint key;
	guint modif;
} KeyPress;


typedef struct
{
	/* the last full search command, including '/' or '?' */
	gchar *search_text;
} CmdContext;


gboolean process_event_cmd_mode(ScintillaObject *sci, CmdContext *ctx, GSList *kpl, GSList *prev_kpl, gboolean *is_repeat);

#endif
