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

/* state commands */
void state_cmd_enter_cmdline_mode(ScintillaObject *sci, CmdContext *ctx, ViState *state);
void state_cmd_enter_insert_mode(ScintillaObject *sci, CmdContext *ctx, ViState *state);
void state_cmd_enter_insert_mode_after(ScintillaObject *sci, CmdContext *ctx, ViState *state);
void state_cmd_enter_insert_mode_line_start(ScintillaObject *sci, CmdContext *ctx, ViState *state);
void state_cmd_enter_insert_mode_line_end(ScintillaObject *sci, CmdContext *ctx, ViState *state);
void state_cmd_enter_insert_mode_next_line(ScintillaObject *sci, CmdContext *ctx, ViState *state);
void state_cmd_enter_insert_mode_prev_line(ScintillaObject *sci, CmdContext *ctx, ViState *state);
void state_cmd_enter_insert_mode_clear_line(ScintillaObject *sci, CmdContext *ctx, ViState *state);
void state_cmd_enter_insert_mode_clear_right(ScintillaObject *sci, CmdContext *ctx, ViState *state);
void state_cmd_enter_insert_mode_delete_char(ScintillaObject *sci, CmdContext *ctx, ViState *state);


void state_cmd_enter_replace_mode(ScintillaObject *sci, CmdContext *ctx, ViState *state);


/* normal commands */
void cmd_page_up(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_page_down(ScintillaObject *sci, CmdContext *ctx, gint num);

void cmd_move_caret_left(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_move_caret_right(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_move_caret_up(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_move_caret_down(ScintillaObject *sci, CmdContext *ctx, gint num);

void cmd_undo(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_redo(ScintillaObject *sci, CmdContext *ctx, gint num);

void cmd_copy_line(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_paste(ScintillaObject *sci, CmdContext *ctx, gint num);

void cmd_search(ScintillaObject *sci, CmdContext *ctx, gint num);

void cmd_delete_line(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_delete_char(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_delete_char_back(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_line(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_line_last(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_join_lines(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_next_word(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_next_word(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_next_word_end(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_previous_word(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_previous_word_end(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_line_start(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_line_end(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_matching_brace(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_doc_percentage(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_screen_top(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_screen_middle(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_goto_screen_bottom(ScintillaObject *sci, CmdContext *ctx, gint num);

void cmd_replace_char(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_uppercase_char(ScintillaObject *sci, CmdContext *ctx, gint num);

void cmd_indent(ScintillaObject *sci, CmdContext *ctx, gint num);
void cmd_unindent(ScintillaObject *sci, CmdContext *ctx, gint num);

#endif
