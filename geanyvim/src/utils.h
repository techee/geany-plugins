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

#define SSM(s, m, w, l) scintilla_send_message((s), (m), (w), (l))
#define NEXT(s, pos) scintilla_send_message((s), SCI_POSITIONAFTER, (pos), 0)
#define PREV(s, pos) scintilla_send_message((s), SCI_POSITIONBEFORE, (pos), 0)
#define REL(s, pos, rel) scintilla_send_message((s), SCI_POSITIONRELATIVE, (pos), (rel))


KeyPress *kp_from_event_key(GdkEventKey *ev);
gchar kp_to_char(KeyPress *kp);
gboolean kp_isdigit(KeyPress *kp);

GSList *kpl_copy(GSList *kpl);
gint kpl_get_int(GSList *kpl, GSList **ret);
void kpl_printf(GSList *kpl);

gchar *get_current_word(ScintillaObject *sci);

void clamp_cursor_pos(ScintillaObject *sci);
void perform_search(ScintillaObject *sci, CmdContext *c, gint num, gboolean invert);

gboolean is_printable(GdkEventKey *ev);

#endif
