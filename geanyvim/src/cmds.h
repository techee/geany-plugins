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

/* "UI" commands */
void ui_cmd_enter_cmdline_mode(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui);
void ui_cmd_enter_insert_mode(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui);
void ui_cmd_enter_insert_mode_after(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui);
void ui_cmd_enter_insert_mode_line_start(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui);
void ui_cmd_enter_insert_mode_line_end(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui);
void ui_cmd_enter_insert_mode_next_line(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui);
void ui_cmd_enter_insert_mode_prev_line(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui);
void ui_cmd_enter_replace_mode(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui);


/* normal commands */
void cmd_page_up(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_page_down(ScintillaObject *sci, ViState *vi_state, gint num);

void cmd_move_caret_left(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_move_caret_right(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_move_caret_up(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_move_caret_down(ScintillaObject *sci, ViState *vi_state, gint num);

void cmd_undo(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_redo(ScintillaObject *sci, ViState *vi_state, gint num);

void cmd_copy_line(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_paste(ScintillaObject *sci, ViState *vi_state, gint num);

void cmd_search(ScintillaObject *sci, ViState *vi_state, gint num);

void cmd_delete_line(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_delete_char(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_line(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_line_last(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_join_lines(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_next_word(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_next_word(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_next_word_end(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_previous_word(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_previous_word_end(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_line_start(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_line_end(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_matching_brace(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_doc_percentage(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_screen_top(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_screen_middle(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_goto_screen_bottom(ScintillaObject *sci, ViState *vi_state, gint num);
void cmd_replace_char(ScintillaObject *sci, ViState *vi_state, gint num);

#endif
