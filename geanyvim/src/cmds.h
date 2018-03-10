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

#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)

/* utils */

void accumulator_append(ViState *vi_state, const gchar *val);
void accumulator_clear(ViState *vi_state);
guint accumulator_len(ViState *vi_state);

ScintillaObject *get_current_doc_sci(void);

void clamp_cursor_pos(ScintillaObject *sci, ViState *vi_state);
gchar *get_current_word(ScintillaObject *sci);
void perform_search(ScintillaObject *sci, ViState *vi_state, gboolean forward);

/* cmds */

void cmd_page_up(ScintillaObject *sci, ViState *vi_state);
void cmd_page_down(ScintillaObject *sci, ViState *vi_state);

void cmd_move_caret_left(ScintillaObject *sci, ViState *vi_state);
void cmd_move_caret_right(ScintillaObject *sci, ViState *vi_state);
void cmd_move_caret_up(ScintillaObject *sci, ViState *vi_state);
void cmd_move_caret_down(ScintillaObject *sci, ViState *vi_state);

void cmd_undo(ScintillaObject *sci, ViState *vi_state);
void cmd_redo(ScintillaObject *sci, ViState *vi_state);

void cmd_copy_line(ScintillaObject *sci, ViState *vi_state);
void cmd_paste(ScintillaObject *sci, ViState *vi_state);

#endif
