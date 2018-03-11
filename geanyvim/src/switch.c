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

#include <gdk/gdkkeysyms.h>
#include <ctype.h>

#include "switch.h"
#include "cmds.h"
#include "utils.h"


static void perform_cmd(Cmd cmd, ScintillaObject *sci, CmdContext *ctx, ViState *state, gint cmd_len)
{
	CmdParams param;
	param.sci = sci;
	param.num = accumulator_get_int(ctx, cmd_len, 1);

	sci_start_undo_action(sci);
	cmd(ctx, &param);
	accumulator_clear(ctx);
	if (state->vi_mode == VI_MODE_COMMAND)
		clamp_cursor_pos(sci, ctx, state);
	sci_end_undo_action(sci);
}

void cmd_switch(GdkEventKey *event, ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	Cmd cmd = NULL;
	gint cmdlen = 1;

	if (event->state & GDK_CONTROL_MASK)
	{
		switch (event->keyval)
		{
			case GDK_KEY_r:
				cmd = cmd_redo;
				break;
			default:
				break;
		}
		if (cmd)
			perform_cmd(cmd, sci, ctx, state, cmdlen);
		return;
	}

	switch (accumulator_previous_char(ctx))
	{
		cmdlen = 2;
		case 'r':
			cmd = cmd_replace_char;
			break;
		default:
			break;
	}

	if (cmd)
	{
		perform_cmd(cmd, sci, ctx, state, cmdlen);
		return;
	}

	switch (event->keyval)
	{
		case GDK_KEY_colon:
		case GDK_KEY_slash:
		case GDK_KEY_question:
			cmd = state_cmd_enter_cmdline_mode;
			break;
		case GDK_KEY_i:
			cmd = state_cmd_enter_insert_mode;
			break;
		case GDK_KEY_R:
			cmd = state_cmd_enter_replace_mode;
			break;
		case GDK_KEY_a:
			cmd = state_cmd_enter_insert_mode_after;
			break;
		case GDK_KEY_I:
			cmd = state_cmd_enter_insert_mode_line_start;
			break;
		case GDK_KEY_A:
			cmd = state_cmd_enter_insert_mode_line_end;
			break;
		case GDK_KEY_o:
			cmd = state_cmd_enter_insert_mode_next_line;
			break;
		case GDK_KEY_S:
			cmd = state_cmd_enter_insert_mode_clear_line;
			break;
		case GDK_KEY_c:
			if (accumulator_previous_char(ctx) == 'c')
			{
				cmdlen = 2;
				cmd = state_cmd_enter_insert_mode_clear_line;
			}
			break;
		case GDK_KEY_O:
			cmd = state_cmd_enter_insert_mode_prev_line;
			break;
		case GDK_KEY_C:
			cmd = state_cmd_enter_insert_mode_clear_right;
			break;
		case GDK_KEY_s:
			cmd = state_cmd_enter_insert_mode_delete_char;
			break;


		case GDK_KEY_Escape:
			accumulator_clear(ctx);
			break;
		case GDK_KEY_Page_Up:
			cmd = cmd_page_up;
			break;
		case GDK_KEY_Page_Down:
			cmd = cmd_page_down;
			break;
		case GDK_KEY_Left:
		case GDK_KEY_leftarrow:
		case GDK_KEY_h:
			cmd = cmd_move_caret_left;
			break;
		case GDK_KEY_Right:
		case GDK_KEY_rightarrow:
		case GDK_KEY_l:
			cmd = cmd_move_caret_right;
			break;
		case GDK_KEY_Down:
		case GDK_KEY_downarrow:
		case GDK_KEY_j:
			cmd = cmd_move_caret_down;
			break;
		case GDK_KEY_Up:
		case GDK_KEY_uparrow:
		case GDK_KEY_k:
			cmd = cmd_move_caret_up;
			break;
		case GDK_KEY_y:
			if (accumulator_previous_char(ctx) == 'y')
			{
				cmdlen = 2;
				cmd = cmd_copy_line;
			}
			break;
		case GDK_KEY_p:
			cmd = cmd_paste;
			break;
		case GDK_KEY_U: // undo on single line - we probably won't implement it
		case GDK_KEY_u:
			cmd = cmd_undo;
			break;
		case GDK_KEY_n:
		case GDK_KEY_N:
			cmd = cmd_search;
			break;
		case GDK_KEY_asterisk:
		case GDK_KEY_numbersign:
		{
			gchar *word = get_current_word(sci);
			g_free(ctx->search_text);
			if (!word)
				ctx->search_text = NULL;
			else
			{
				const gchar *prefix = event->keyval == GDK_KEY_asterisk ? "/" : "?";
				ctx->search_text = g_strconcat(prefix, word, NULL);
			}
			g_free(word);
			cmd = cmd_search;
			break;
		}
		case GDK_KEY_D:
			cmd = cmd_delete_line;
			break;
		case GDK_KEY_d:
			if (accumulator_previous_char(ctx) == 'd')
			{
				cmdlen = 2;
				cmd = cmd_delete_line;
			}
			break;
		case GDK_KEY_x:
			cmd = cmd_delete_char;
			break;
		case GDK_KEY_X:
			cmd = cmd_delete_char_back;
			break;
		case GDK_KEY_G:
			cmd = cmd_goto_line_last;
			break;
		case GDK_KEY_g:
			if (accumulator_previous_char(ctx) == 'g')
			{
				cmdlen = 2;
				cmd = cmd_goto_line;
			}
			break;
		case GDK_KEY_J:
			cmd = cmd_join_lines;
			break;
		case GDK_KEY_w:
		case GDK_KEY_W:
			cmd = cmd_goto_next_word;
			break;
		case GDK_KEY_e:
		case GDK_KEY_E:
			cmd = cmd_goto_next_word_end;
			break;
		case GDK_KEY_b:
			cmd = cmd_goto_previous_word;
			break;
		case GDK_KEY_B:
			cmd = cmd_goto_previous_word_end;
			break;
		case GDK_KEY_0:
			if (!isdigit(accumulator_previous_char(ctx)))
				cmd = cmd_goto_line_start;
			break;
		case GDK_KEY_dollar:
			cmd = cmd_goto_line_end;
			break;
		case GDK_KEY_percent:
			if (accumulator_len(ctx) > 1 && accumulator_get_int(ctx, 1, -1) != -1)
				cmd = cmd_goto_doc_percentage;
			else
				cmd = cmd_goto_matching_brace;
			break;
		case GDK_KEY_H:
			cmd = cmd_goto_screen_top;
			break;
		case GDK_KEY_M:
			cmd = cmd_goto_screen_middle;
			break;
		case GDK_KEY_L:
			cmd = cmd_goto_screen_bottom;
			break;
		case GDK_KEY_asciitilde:
			cmd = cmd_uppercase_char;
			break;
		case GDK_KEY_less:
			if (accumulator_previous_char(ctx) == '<')
			{
				cmdlen = 2;
				cmd = cmd_unindent;
			}
			break;
		case GDK_KEY_greater:
			if (accumulator_previous_char(ctx) == '>')
			{
				cmdlen = 2;
				cmd = cmd_indent;
			}
			break;
		//tx, Tx - like above but stop one character before
		default:
			break;
	}

	if (cmd)
		perform_cmd(cmd, sci, ctx, state, cmdlen);
}
