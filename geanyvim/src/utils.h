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

#define SET_POS(s, pos, scr) set_current_position((s), (pos), (scr))
#define GET_CUR_LINE(s) scintilla_send_message((s), SCI_LINEFROMPOSITION, \
	SSM((s), SCI_GETCURRENTPOS, 0, 0), 0)

#define MAX_CHAR_SIZE 16

KeyPress *kp_from_event_key(GdkEventKey *ev);
const gchar *kp_to_str(KeyPress *kp);
gboolean kp_isdigit(KeyPress *kp);

GSList *kpl_copy(GSList *kpl);
gint kpl_get_int(GSList *kpl, GSList **ret);
void kpl_printf(GSList *kpl);

gchar *get_current_word(ScintillaObject *sci);

void clamp_cursor_pos(ScintillaObject *sci);
void perform_search(CmdContext *c, gint num, gboolean invert);

gboolean is_printable(GdkEventKey *ev);

void set_current_position(ScintillaObject *sci, gint position, gboolean scroll_to_caret);

#endif
