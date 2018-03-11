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


typedef struct
{
	/* the last full search command, including '/' or '?' */
	gchar *search_text;

	/* input accumulated over time (e.g. for commands like 100dd) */
	gchar *accumulator;
} CmdContext;


typedef struct
{
	/* current Scintilla object */
	ScintillaObject *sci;
	/* number preceding command */
	gint num;
} CmdParams;


typedef void (*Cmd)(CmdContext *c, CmdParams *p);

/* state commands */
void state_cmd_enter_cmdline_mode(CmdContext *c, CmdParams *p);
void state_cmd_enter_insert_mode(CmdContext *c, CmdParams *p);
void state_cmd_enter_insert_mode_after(CmdContext *c, CmdParams *p);
void state_cmd_enter_insert_mode_line_start(CmdContext *c, CmdParams *p);
void state_cmd_enter_insert_mode_line_end(CmdContext *c, CmdParams *p);
void state_cmd_enter_insert_mode_next_line(CmdContext *c, CmdParams *p);
void state_cmd_enter_insert_mode_prev_line(CmdContext *c, CmdParams *p);
void state_cmd_enter_insert_mode_clear_line(CmdContext *c, CmdParams *p);
void state_cmd_enter_insert_mode_clear_right(CmdContext *c, CmdParams *p);
void state_cmd_enter_insert_mode_delete_char(CmdContext *c, CmdParams *p);


void state_cmd_enter_replace_mode(CmdContext *c, CmdParams *p);


/* normal commands */

void cmd_page_up(CmdContext *c, CmdParams *p);
void cmd_page_down(CmdContext *c, CmdParams *p);

void cmd_move_caret_left(CmdContext *c, CmdParams *p);
void cmd_move_caret_right(CmdContext *c, CmdParams *p);
void cmd_move_caret_up(CmdContext *c, CmdParams *p);
void cmd_move_caret_down(CmdContext *c, CmdParams *p);

void cmd_undo(CmdContext *c, CmdParams *p);
void cmd_redo(CmdContext *c, CmdParams *p);

void cmd_copy_line(CmdContext *c, CmdParams *p);
void cmd_paste(CmdContext *c, CmdParams *p);

void cmd_search(CmdContext *c, CmdParams *p);

void cmd_delete_line(CmdContext *c, CmdParams *p);
void cmd_delete_char(CmdContext *c, CmdParams *p);
void cmd_delete_char_back(CmdContext *c, CmdParams *p);
void cmd_goto_line(CmdContext *c, CmdParams *p);
void cmd_goto_line_last(CmdContext *c, CmdParams *p);
void cmd_join_lines(CmdContext *c, CmdParams *p);
void cmd_goto_next_word(CmdContext *c, CmdParams *p);
void cmd_goto_next_word(CmdContext *c, CmdParams *p);
void cmd_goto_next_word_end(CmdContext *c, CmdParams *p);
void cmd_goto_previous_word(CmdContext *c, CmdParams *p);
void cmd_goto_previous_word_end(CmdContext *c, CmdParams *p);
void cmd_goto_line_start(CmdContext *c, CmdParams *p);
void cmd_goto_line_end(CmdContext *c, CmdParams *p);
void cmd_goto_matching_brace(CmdContext *c, CmdParams *p);
void cmd_goto_doc_percentage(CmdContext *c, CmdParams *p);
void cmd_goto_screen_top(CmdContext *c, CmdParams *p);
void cmd_goto_screen_middle(CmdContext *c, CmdParams *p);
void cmd_goto_screen_bottom(CmdContext *c, CmdParams *p);

void cmd_replace_char(CmdContext *c, CmdParams *p);
void cmd_uppercase_char(CmdContext *c, CmdParams *p);

void cmd_indent(CmdContext *c, CmdParams *p);
void cmd_unindent(CmdContext *c, CmdParams *p);

#endif
