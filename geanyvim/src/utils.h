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

#ifndef __GEANYVIM_UTILS_H__
#define __GEANYVIM_UTILS_H__

#include <glib.h>

#include "cmds.h"

#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)

KeyPress *kp_from_event_key(GdkEventKey *ev);
gchar kp_to_char(KeyPress *kp);
gboolean kp_isdigit(KeyPress *kp);

KpList *kpl_copy(KpList *kpl);
gint kp_get_int(KpList *kpl, gint start_pos, gint default_val);

ScintillaObject *get_current_doc_sci(void);
gchar *get_current_word(ScintillaObject *sci);

void clamp_cursor_pos(ScintillaObject *sci);
void perform_search(ScintillaObject *sci, CmdContext *ctx, gboolean forward);

#endif
