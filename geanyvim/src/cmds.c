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

void state_cmd_enter_cmdline_mode(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	enter_cmdline_mode();
}

void state_cmd_enter_insert_mode(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	state->vi_mode = VI_MODE_INSERT;
	prepare_vi_mode(sci, ctx, state);
}

void state_cmd_enter_replace_mode(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	state->vi_mode = VI_MODE_REPLACE;
	prepare_vi_mode(sci, ctx, state);
}

void state_cmd_enter_insert_mode_after(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	gint pos = sci_get_current_position(sci);
	gint end_pos = sci_get_line_end_position(sci, sci_get_current_line(sci));
	if (pos < end_pos)
		sci_send_command(sci, SCI_CHARRIGHT);

	state_cmd_enter_insert_mode(sci, ctx, state);
}

void state_cmd_enter_insert_mode_line_start(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	gint pos, line;
	sci_send_command(sci, SCI_HOME);
	pos = sci_get_current_position(sci);
	line = sci_get_current_line(sci);
	while (isspace(sci_get_char_at(sci, pos)))
	{
		if (sci_get_line_from_position(sci, pos + 1) != line)
			break;
		pos++;
	}
	sci_set_current_position(sci, pos, TRUE);
	state_cmd_enter_insert_mode(sci, ctx, state);
}

void state_cmd_enter_insert_mode_line_end(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	sci_send_command(sci, SCI_LINEEND);
	state_cmd_enter_insert_mode(sci, ctx, state);
}

void state_cmd_enter_insert_mode_next_line(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	sci_send_command(sci, SCI_LINEEND);
	sci_send_command(sci, SCI_NEWLINE);
	state_cmd_enter_insert_mode(sci, ctx, state);
}

void state_cmd_enter_insert_mode_prev_line(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	sci_send_command(sci, SCI_HOME);
	sci_send_command(sci, SCI_NEWLINE);
	sci_send_command(sci, SCI_LINEUP);
	state_cmd_enter_insert_mode(sci, ctx, state);
}

void state_cmd_enter_insert_mode_clear_line(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	sci_send_command(sci, SCI_DELLINELEFT);
	sci_send_command(sci, SCI_DELLINERIGHT);
	state_cmd_enter_insert_mode(sci, ctx, state);
}

void state_cmd_enter_insert_mode_clear_right(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	sci_send_command(sci, SCI_DELLINERIGHT);
	state_cmd_enter_insert_mode(sci, ctx, state);
}

void state_cmd_enter_insert_mode_delete_char(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	gint pos = sci_get_current_position(sci);
	SSM(sci, SCI_DELETERANGE, pos, 1);
	state_cmd_enter_insert_mode(sci, ctx, state);
}


/* normal commands */

void cmd_page_up(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_PAGEUP);
}

void cmd_page_down(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_PAGEDOWN);
}

void cmd_move_caret_left(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint pos = sci_get_current_position(sci);
	gint start_pos = sci_get_position_from_line(sci, sci_get_current_line(sci));
	gint i;
	for (i = 0; i < num && pos > start_pos; i++)
		sci_send_command(sci, SCI_CHARLEFT);
}

void cmd_move_caret_right(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint pos = sci_get_current_position(sci);
	gint end_pos = sci_get_line_end_position(sci, sci_get_current_line(sci));
	gint i;
	for (i = 0; i < num && pos < end_pos - 1; i++)
		sci_send_command(sci, SCI_CHARRIGHT);
}

void cmd_move_caret_up(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_LINEUP);
}

void cmd_move_caret_down(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_LINEDOWN);
}

void cmd_undo(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
	{
		if (SSM(sci, SCI_CANUNDO, 0, 0))
			SSM(sci, SCI_UNDO, 0, 0);
		else
			break;
	}
}

void cmd_redo(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
	{
		if (SSM(sci, SCI_CANREDO, 0, 0))
			SSM(sci, SCI_REDO, 0, 0);
		else
			break;
	}
}

void cmd_copy_line(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint start = sci_get_position_from_line(sci, sci_get_current_line(sci));
	gint end = sci_get_position_from_line(sci, sci_get_current_line(sci) + num);
	SSM(sci, SCI_COPYRANGE, start, end);
}

void cmd_paste(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint pos = sci_get_position_from_line(sci, sci_get_current_line(sci)+1);
	gint i;
	sci_set_current_position(sci, pos, TRUE);
	for (i = 0; i < num; i++)
		SSM(sci, SCI_PASTE, 0, 0);
	sci_set_current_position(sci, pos, TRUE);
}

void cmd_delete_line(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint start = sci_get_position_from_line(sci, sci_get_current_line(sci));
	gint end = sci_get_position_from_line(sci, sci_get_current_line(sci) + num);
	SSM(sci, SCI_DELETERANGE, start, end-start);
}

void cmd_search(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gboolean forward = TRUE;
	char last;
	gint i;

	if (!ctx->accumulator)
		return;

	last = accumulator_current_char(ctx);

	if (last == 'N')
		forward = FALSE;

	for (i = 0; i < num; i++)
		perform_search(sci, ctx, forward);
}

void cmd_delete_char(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint pos = sci_get_current_position(sci);
	gint i;
	for (i = 0; i < num; i++)
		SSM(sci, SCI_DELETERANGE, pos, 1);
}

void cmd_delete_char_back(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint pos = sci_get_current_position(sci);
	gint i;
	for (i = 0; i < num; i++)
		SSM(sci, SCI_DELETERANGE, pos-1, 1);
}

void cmd_goto_line(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint line_num = sci_get_line_count(sci);
	gint pos;

	num = num > line_num ? line_num : num;
	pos = sci_get_position_from_line(sci, num - 1);
	sci_set_current_position(sci, pos, TRUE);
}

void cmd_goto_line_last(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint line_num = sci_get_line_count(sci);

	/* override default line number to the end of file when number not entered */
	num = accumulator_get_int(ctx, 1, line_num);
	cmd_goto_line(sci, ctx, num);
}

void cmd_join_lines(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint line = sci_get_current_line(sci);
	gint next_line_pos = sci_get_position_from_line(sci, line+num);

	sci_set_target_start(sci, sci_get_current_position(sci));
	sci_set_target_end(sci, next_line_pos);
	SSM(sci, SCI_LINESJOIN, 0, 0);
}

void cmd_goto_next_word(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_WORDRIGHT);
}

void cmd_goto_next_word_end(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_WORDRIGHTEND);
}

void cmd_goto_previous_word(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_WORDLEFT);
}

void cmd_goto_previous_word_end(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_WORDLEFTEND);
}

void cmd_goto_line_start(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	sci_send_command(sci, SCI_HOME);
}

void cmd_goto_line_end(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
	{
		sci_send_command(sci, SCI_LINEEND);
		if (i != num - 1)
			sci_set_current_position(sci, sci_get_current_position(sci) + 1, TRUE);
	}
}

void cmd_goto_matching_brace(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint pos = sci_get_current_position(sci);
	pos = SSM(sci, SCI_BRACEMATCH, pos, 0);
	if (pos != -1)
		sci_set_current_position(sci, pos, TRUE);
}

void cmd_goto_doc_percentage(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint line_num = sci_get_line_count(sci);
	gint pos;

	if (num > 100)
		num = 100;

	line_num = (line_num * num) / 100;
	pos = sci_get_position_from_line(sci, line_num);
	sci_set_current_position(sci, pos, TRUE);
}

void cmd_goto_screen_top(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint top = SSM(sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	gint pos = sci_get_position_from_line(sci, top+num-1);
	sci_set_current_position(sci, pos, TRUE);
}

void cmd_goto_screen_middle(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint top = SSM(sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	gint count = SSM(sci, SCI_LINESONSCREEN, 0, 0);
	gint pos = sci_get_position_from_line(sci, top+count/2);
	sci_set_current_position(sci, pos, TRUE);
}

void cmd_goto_screen_bottom(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint top = SSM(sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	gint count = SSM(sci, SCI_LINESONSCREEN, 0, 0);
	gint pos = sci_get_position_from_line(sci, top+count-num);
	sci_set_current_position(sci, pos, TRUE);
}

void cmd_replace_char(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint pos = sci_get_current_position(sci);
	gchar *repl = ctx->accumulator + accumulator_len(ctx) - 1;

	sci_set_target_start(sci, pos);
	sci_set_target_end(sci, pos + 1);
	sci_replace_target(sci, repl, FALSE);
}

void cmd_uppercase_char(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	//TODO: for some reason we don't get the same cursor position after undoing
	gint pos = sci_get_current_position(sci);
	gchar upper[2] = {toupper(sci_get_char_at(sci, pos)), '\0'};

	sci_set_target_start(sci, pos);
	sci_set_target_end(sci, pos + 1);
	sci_replace_target(sci, upper, FALSE);
	sci_send_command(sci, SCI_CHARRIGHT);
}

void cmd_indent(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint pos = sci_get_current_position(sci);;
	gint i;

	for (i = 0; i < num; i++)
	{
		sci_send_command(sci, SCI_HOME);
		sci_send_command(sci, SCI_TAB);
		if (i == 0)
			pos = sci_get_current_position(sci);
		sci_send_command(sci, SCI_LINEDOWN);
	}
	sci_set_current_position(sci, pos, FALSE);
}

void cmd_unindent(ScintillaObject *sci, CmdContext *ctx, gint num)
{
	gint pos = sci_get_current_position(sci);;
	gint i;

	for (i = 0; i < num; i++)
	{
		sci_send_command(sci, SCI_HOME);
		sci_send_command(sci, SCI_BACKTAB);
		if (i == 0)
			pos = sci_get_current_position(sci);
		sci_send_command(sci, SCI_LINEDOWN);
	}
	sci_set_current_position(sci, pos, FALSE);
}
