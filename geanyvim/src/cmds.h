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

#include <gtk/gtk.h>
#include "Scintilla.h"
#include "ScintillaWidget.h"

#include "vi.h"

#define INSERT_BUF_LEN 4096

typedef struct
{
	guint key;
	guint modif;
} KeyPress;


typedef struct
{
	/* the last full search command, including '/' or '?' */
	gchar *search_text;
	/* the last full character search command, such as 'fc' or 'Tc' */
	gchar *search_char;
	/* whether the last copy was in line-copy-mode (like yy) or selection mode */
	gboolean line_copy;
	/* selection anchor - selection is between anchor and caret */
	gint sel_anchor;
	/* number entered before performing mode-switching command */
	gint num;
	/* buffer used in insert/replace mode to record entered text so it can be
	 * copied N times when e.g. 'i' is preceded by a number */
	gchar insert_buf[INSERT_BUF_LEN];
	gint insert_buf_len;
	/* whether insert mode was entered using 'o' or 'O' - we need to add newlines
	 * when copying it N times */
	gboolean newline_insert;
	/* callbacks for the backend */
	ViCallback *cb;
	/* current scintilla object */
	ScintillaObject *sci;
} CmdContext;


gboolean process_event_cmd_mode(CmdContext *ctx, GSList *kpl,
	GSList *prev_kpl, gboolean *is_repeat, gboolean *consumed);
gboolean process_event_vis_mode(CmdContext *ctx, GSList *kpl, gboolean *consumed);
gboolean process_event_ins_mode(CmdContext *ctx, GSList *kpl, gboolean *consumed);

#endif
