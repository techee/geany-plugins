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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>
#include <ctype.h>

#include "state.h"
#include "cmds.h"
#include "utils.h"


/* state commands */

void state_cmd_enter_cmdline_mode(CmdContext *c, CmdParams *p)
{
	set_vi_mode(VI_MODE_CMDLINE, p->sci);
}

void state_cmd_enter_insert_mode(CmdContext *c, CmdParams *p)
{
	set_vi_mode(VI_MODE_INSERT, p->sci);
}

void state_cmd_enter_replace_mode(CmdContext *c, CmdParams *p)
{
	set_vi_mode(VI_MODE_REPLACE, p->sci);
}

void state_cmd_enter_insert_mode_after(CmdContext *c, CmdParams *p)
{
	gint pos = sci_get_current_position(p->sci);
	gint end_pos = sci_get_line_end_position(p->sci, sci_get_current_line(p->sci));
	if (pos < end_pos)
		sci_send_command(p->sci, SCI_CHARRIGHT);

	state_cmd_enter_insert_mode(c, p);
}

void state_cmd_enter_insert_mode_line_start(CmdContext *c, CmdParams *p)
{
	gint pos, line;
	sci_send_command(p->sci, SCI_HOME);
	pos = sci_get_current_position(p->sci);
	line = sci_get_current_line(p->sci);
	while (isspace(sci_get_char_at(p->sci, pos)))
	{
		if (sci_get_line_from_position(p->sci, pos + 1) != line)
			break;
		pos++;
	}
	sci_set_current_position(p->sci, pos, TRUE);
	state_cmd_enter_insert_mode(c, p);
}

void state_cmd_enter_insert_mode_line_end(CmdContext *c, CmdParams *p)
{
	sci_send_command(p->sci, SCI_LINEEND);
	state_cmd_enter_insert_mode(c, p);
}

void state_cmd_enter_insert_mode_next_line(CmdContext *c, CmdParams *p)
{
	sci_send_command(p->sci, SCI_LINEEND);
	sci_send_command(p->sci, SCI_NEWLINE);
	state_cmd_enter_insert_mode(c, p);
}

void state_cmd_enter_insert_mode_prev_line(CmdContext *c, CmdParams *p)
{
	sci_send_command(p->sci, SCI_HOME);
	sci_send_command(p->sci, SCI_NEWLINE);
	sci_send_command(p->sci, SCI_LINEUP);
	state_cmd_enter_insert_mode(c, p);
}

void state_cmd_enter_insert_mode_clear_line(CmdContext *c, CmdParams *p)
{
	sci_send_command(p->sci, SCI_DELLINELEFT);
	sci_send_command(p->sci, SCI_DELLINERIGHT);
	state_cmd_enter_insert_mode(c, p);
}

void state_cmd_enter_insert_mode_clear_right(CmdContext *c, CmdParams *p)
{
	sci_send_command(p->sci, SCI_DELLINERIGHT);
	state_cmd_enter_insert_mode(c, p);
}

void state_cmd_enter_insert_mode_delete_char(CmdContext *c, CmdParams *p)
{
	gint pos = sci_get_current_position(p->sci);
	SSM(p->sci, SCI_DELETERANGE, pos, 1);
	state_cmd_enter_insert_mode(c, p);
}


/* normal commands */

void cmd_page_up(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		sci_send_command(p->sci, SCI_PAGEUP);
}

void cmd_page_down(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		sci_send_command(p->sci, SCI_PAGEDOWN);
}

void cmd_move_caret_left(CmdContext *c, CmdParams *p)
{
	gint pos = sci_get_current_position(p->sci);
	gint start_pos = sci_get_position_from_line(p->sci, sci_get_current_line(p->sci));
	gint i;
	for (i = 0; i < p->num && pos > start_pos; i++)
		sci_send_command(p->sci, SCI_CHARLEFT);
}

void cmd_move_caret_right(CmdContext *c, CmdParams *p)
{
	gint pos = sci_get_current_position(p->sci);
	gint end_pos = sci_get_line_end_position(p->sci, sci_get_current_line(p->sci));
	gint i;
	for (i = 0; i < p->num && pos < end_pos - 1; i++)
		sci_send_command(p->sci, SCI_CHARRIGHT);
}

void cmd_move_caret_up(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		sci_send_command(p->sci, SCI_LINEUP);
}

void cmd_move_caret_down(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		sci_send_command(p->sci, SCI_LINEDOWN);
}

void cmd_undo(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
	{
		if (SSM(p->sci, SCI_CANUNDO, 0, 0))
			SSM(p->sci, SCI_UNDO, 0, 0);
		else
			break;
	}
}

void cmd_redo(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
	{
		if (SSM(p->sci, SCI_CANREDO, 0, 0))
			SSM(p->sci, SCI_REDO, 0, 0);
		else
			break;
	}
}

void cmd_copy_line(CmdContext *c, CmdParams *p)
{
	gint start = sci_get_position_from_line(p->sci, sci_get_current_line(p->sci));
	gint end = sci_get_position_from_line(p->sci, sci_get_current_line(p->sci) + p->num);
	SSM(p->sci, SCI_COPYRANGE, start, end);
}

void cmd_paste(CmdContext *c, CmdParams *p)
{
	gint pos = sci_get_position_from_line(p->sci, sci_get_current_line(p->sci)+1);
	gint i;
	sci_set_current_position(p->sci, pos, TRUE);
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_PASTE, 0, 0);
	sci_set_current_position(p->sci, pos, TRUE);
}

void cmd_delete_line(CmdContext *c, CmdParams *p)
{
	gint start = sci_get_position_from_line(p->sci, sci_get_current_line(p->sci));
	gint end = sci_get_position_from_line(p->sci, sci_get_current_line(p->sci) + p->num);
	SSM(p->sci, SCI_DELETERANGE, start, end-start);
}

void cmd_search(CmdContext *c, CmdParams *p)
{
	gboolean forward = TRUE;
	char last;
	gint i;

	if (!c->accumulator)
		return;

	last = accumulator_current_char(c);

	if (last == 'N')
		forward = FALSE;

	for (i = 0; i < p->num; i++)
		perform_search(p->sci, c, forward);
}

void cmd_delete_char(CmdContext *c, CmdParams *p)
{
	gint pos = sci_get_current_position(p->sci);
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_DELETERANGE, pos, 1);
}

void cmd_delete_char_back(CmdContext *c, CmdParams *p)
{
	gint pos = sci_get_current_position(p->sci);
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_DELETERANGE, pos-1, 1);
}

void cmd_goto_line(CmdContext *c, CmdParams *p)
{
	gint line_num = sci_get_line_count(p->sci);
	gint num = p->num > line_num ? line_num : p->num;
	gint pos;
	
	pos = sci_get_position_from_line(p->sci, num - 1);
	sci_set_current_position(p->sci, pos, TRUE);
}

void cmd_goto_line_last(CmdContext *c, CmdParams *p)
{
	gint line_num = sci_get_line_count(p->sci);
	/* override default line number to the end of file when number not entered */
	p->num = accumulator_get_int(c, 1, line_num);
	cmd_goto_line(c, p);
}

void cmd_join_lines(CmdContext *c, CmdParams *p)
{
	gint line = sci_get_current_line(p->sci);
	gint next_line_pos = sci_get_position_from_line(p->sci, line+p->num);

	sci_set_target_start(p->sci, sci_get_current_position(p->sci));
	sci_set_target_end(p->sci, next_line_pos);
	SSM(p->sci, SCI_LINESJOIN, 0, 0);
}

void cmd_goto_next_word(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		sci_send_command(p->sci, SCI_WORDRIGHT);
}

void cmd_goto_next_word_end(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		sci_send_command(p->sci, SCI_WORDRIGHTEND);
}

void cmd_goto_previous_word(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		sci_send_command(p->sci, SCI_WORDLEFT);
}

void cmd_goto_previous_word_end(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		sci_send_command(p->sci, SCI_WORDLEFTEND);
}

void cmd_goto_line_start(CmdContext *c, CmdParams *p)
{
	sci_send_command(p->sci, SCI_HOME);
}

void cmd_goto_line_end(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
	{
		sci_send_command(p->sci, SCI_LINEEND);
		if (i != p->num - 1)
			sci_set_current_position(p->sci, sci_get_current_position(p->sci) + 1, TRUE);
	}
}

void cmd_goto_matching_brace(CmdContext *c, CmdParams *p)
{
	gint pos = sci_get_current_position(p->sci);
	pos = SSM(p->sci, SCI_BRACEMATCH, pos, 0);
	if (pos != -1)
		sci_set_current_position(p->sci, pos, TRUE);
}

void cmd_goto_doc_percentage(CmdContext *c, CmdParams *p)
{
	gint line_num = sci_get_line_count(p->sci);
	gint pos;

	if (p->num > 100)
		p->num = 100;

	line_num = (line_num * p->num) / 100;
	pos = sci_get_position_from_line(p->sci, line_num);
	sci_set_current_position(p->sci, pos, TRUE);
}

void cmd_goto_screen_top(CmdContext *c, CmdParams *p)
{
	gint top = SSM(p->sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	gint pos = sci_get_position_from_line(p->sci, top+p->num-1);
	sci_set_current_position(p->sci, pos, TRUE);
}

void cmd_goto_screen_middle(CmdContext *c, CmdParams *p)
{
	gint top = SSM(p->sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	gint count = SSM(p->sci, SCI_LINESONSCREEN, 0, 0);
	gint pos = sci_get_position_from_line(p->sci, top+count/2);
	sci_set_current_position(p->sci, pos, TRUE);
}

void cmd_goto_screen_bottom(CmdContext *c, CmdParams *p)
{
	gint top = SSM(p->sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	gint count = SSM(p->sci, SCI_LINESONSCREEN, 0, 0);
	gint pos = sci_get_position_from_line(p->sci, top+count-p->num);
	sci_set_current_position(p->sci, pos, TRUE);
}

void cmd_replace_char(CmdContext *c, CmdParams *p)
{
	gint pos = sci_get_current_position(p->sci);
	gchar *repl = c->accumulator + accumulator_len(c) - 1;

	sci_set_target_start(p->sci, pos);
	sci_set_target_end(p->sci, pos + 1);
	sci_replace_target(p->sci, repl, FALSE);
}

void cmd_uppercase_char(CmdContext *c, CmdParams *p)
{
	//TODO: for some reason we don't get the same cursor position after undoing
	gint pos = sci_get_current_position(p->sci);
	gchar upper[2] = {toupper(sci_get_char_at(p->sci, pos)), '\0'};

	sci_set_target_start(p->sci, pos);
	sci_set_target_end(p->sci, pos + 1);
	sci_replace_target(p->sci, upper, FALSE);
	sci_send_command(p->sci, SCI_CHARRIGHT);
}

void cmd_indent(CmdContext *c, CmdParams *p)
{
	gint pos = sci_get_current_position(p->sci);
	gint i;

	for (i = 0; i < p->num; i++)
	{
		sci_send_command(p->sci, SCI_HOME);
		sci_send_command(p->sci, SCI_TAB);
		if (i == 0)
			pos = sci_get_current_position(p->sci);
		sci_send_command(p->sci, SCI_LINEDOWN);
	}
	sci_set_current_position(p->sci, pos, FALSE);
}

void cmd_unindent(CmdContext *c, CmdParams *p)
{
	gint pos = sci_get_current_position(p->sci);
	gint i;

	for (i = 0; i < p->num; i++)
	{
		sci_send_command(p->sci, SCI_HOME);
		sci_send_command(p->sci, SCI_BACKTAB);
		if (i == 0)
			pos = sci_get_current_position(p->sci);
		sci_send_command(p->sci, SCI_LINEDOWN);
	}
	sci_set_current_position(p->sci, pos, FALSE);
}
