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


static void perform_cmd_generic(void (*func)(ScintillaObject *sci, CmdContext *ctx, int num), ScintillaObject *sci, CmdContext *ctx, ViState *state, gint cmd_len)
{
	gint num = accumulator_get_int(ctx, cmd_len, 1);
	sci_start_undo_action(sci);
	func(sci, ctx, num);
	accumulator_clear(ctx);
	if (state->vi_mode == VI_MODE_COMMAND)
		clamp_cursor_pos(sci, ctx, state);
	sci_end_undo_action(sci);
}

static void perform_cmd(void (*func)(ScintillaObject *sci, CmdContext *ctx, int num), ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	perform_cmd_generic(func, sci, ctx, state, 1);
}

static void perform_cmd_2(void (*func)(ScintillaObject *sci, CmdContext *ctx, int num), ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	perform_cmd_generic(func, sci, ctx, state, 2);
}

static void perform_state_cmd(void (*func)(ScintillaObject *sci, CmdContext *ctx, ViState *state), ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	sci_start_undo_action(sci);
	func(sci, ctx, state);
	accumulator_clear(ctx);
	if (state->vi_mode == VI_MODE_COMMAND)
		clamp_cursor_pos(sci, ctx, state);
	sci_end_undo_action(sci);
}

void cmd_switch(GdkEventKey *event, ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	gboolean handled = TRUE;
	switch (accumulator_previous_char(ctx))
	{
		case 'r':
			perform_cmd(cmd_replace_char, sci, ctx, state);
			break;
		default:
			handled = FALSE;
	}

	if (handled)
		return;

	if (event->state & GDK_CONTROL_MASK)
	{
		switch (event->keyval)
		{
			case GDK_KEY_r:
				perform_cmd(cmd_redo, sci, ctx, state);
				break;
			default:
				break;
		}
		return;
	}

	switch (event->keyval)
	{
		case GDK_KEY_colon:
		case GDK_KEY_slash:
		case GDK_KEY_question:
			perform_state_cmd(state_cmd_enter_cmdline_mode, sci, ctx, state);
			break;
		case GDK_KEY_i:
			perform_state_cmd(state_cmd_enter_insert_mode, sci, ctx, state);
			break;
		case GDK_KEY_R:
			perform_state_cmd(state_cmd_enter_replace_mode, sci, ctx, state);
			break;
		case GDK_KEY_a:
			perform_state_cmd(state_cmd_enter_insert_mode_after, sci, ctx, state);
			break;
		case GDK_KEY_I:
			perform_state_cmd(state_cmd_enter_insert_mode_line_start, sci, ctx, state);
			break;
		case GDK_KEY_A:
			perform_state_cmd(state_cmd_enter_insert_mode_line_end, sci, ctx, state);
			break;
		case GDK_KEY_o:
			perform_state_cmd(state_cmd_enter_insert_mode_next_line, sci, ctx, state);
			break;
		case GDK_KEY_S:
			perform_state_cmd(state_cmd_enter_insert_mode_clear_line, sci, ctx, state);
			break;
		case GDK_KEY_c:
			if (accumulator_previous_char(ctx) == 'c')
				perform_state_cmd(state_cmd_enter_insert_mode_clear_line, sci, ctx, state);
			break;
		case GDK_KEY_O:
			perform_state_cmd(state_cmd_enter_insert_mode_prev_line, sci, ctx, state);
			break;
		case GDK_KEY_C:
			perform_state_cmd(state_cmd_enter_insert_mode_clear_right, sci, ctx, state);
			break;
		case GDK_KEY_s:
			perform_state_cmd(state_cmd_enter_insert_mode_delete_char, sci, ctx, state);
			break;


		case GDK_KEY_Escape:
			accumulator_clear(ctx);
			break;
		case GDK_KEY_Page_Up:
			perform_cmd(cmd_page_up, sci, ctx, state);
			break;
		case GDK_KEY_Page_Down:
			perform_cmd(cmd_page_down, sci, ctx, state);
			break;
		case GDK_KEY_Left:
		case GDK_KEY_leftarrow:
		case GDK_KEY_h:
			perform_cmd(cmd_move_caret_left, sci, ctx, state);
			break;
		case GDK_KEY_Right:
		case GDK_KEY_rightarrow:
		case GDK_KEY_l:
			perform_cmd(cmd_move_caret_right, sci, ctx, state);
			break;
		case GDK_KEY_Down:
		case GDK_KEY_downarrow:
		case GDK_KEY_j:
			perform_cmd(cmd_move_caret_down, sci, ctx, state);
			break;
		case GDK_KEY_Up:
		case GDK_KEY_uparrow:
		case GDK_KEY_k:
			perform_cmd(cmd_move_caret_up, sci, ctx, state);
			break;
		case GDK_KEY_y:
			if (accumulator_previous_char(ctx) == 'y')
				perform_cmd_2(cmd_copy_line, sci, ctx, state);
			break;
		case GDK_KEY_p:
			perform_cmd(cmd_paste, sci, ctx, state);
			break;
		case GDK_KEY_U: // undo on single line - we probably won't implement it
		case GDK_KEY_u:
			perform_cmd(cmd_undo, sci, ctx, state);
			break;
		case GDK_KEY_n:
		case GDK_KEY_N:
			perform_cmd(cmd_search, sci, ctx, state);
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
			perform_cmd(cmd_search, sci, ctx, state);
			break;
		}
		case GDK_KEY_D:
			perform_cmd(cmd_delete_line, sci, ctx, state);
			break;
		case GDK_KEY_d:
			if (accumulator_previous_char(ctx) == 'd')
				perform_cmd_2(cmd_delete_line, sci, ctx, state);
			break;
		case GDK_KEY_x:
			perform_cmd(cmd_delete_char, sci, ctx, state);
			break;
		case GDK_KEY_X:
			perform_cmd(cmd_delete_char_back, sci, ctx, state);
			break;
		case GDK_KEY_G:
			perform_cmd(cmd_goto_line_last, sci, ctx, state);
			break;
		case GDK_KEY_g:
			if (accumulator_previous_char(ctx) == 'g')
				perform_cmd_2(cmd_goto_line, sci, ctx, state);
			break;
		case GDK_KEY_J:
			perform_cmd(cmd_join_lines, sci, ctx, state);
			break;
		case GDK_KEY_w:
		case GDK_KEY_W:
			perform_cmd(cmd_goto_next_word, sci, ctx, state);
			break;
		case GDK_KEY_e:
		case GDK_KEY_E:
			perform_cmd(cmd_goto_next_word_end, sci, ctx, state);
			break;
		case GDK_KEY_b:
			perform_cmd(cmd_goto_previous_word, sci, ctx, state);
			break;
		case GDK_KEY_B:
			perform_cmd(cmd_goto_previous_word_end, sci, ctx, state);
			break;
		case GDK_KEY_0:
			if (!isdigit(accumulator_previous_char(ctx)))
				perform_cmd(cmd_goto_line_start, sci, ctx, state);
			break;
		case GDK_KEY_dollar:
			perform_cmd(cmd_goto_line_end, sci, ctx, state);
			break;
		case GDK_KEY_percent:
			if (accumulator_len(ctx) > 1 && accumulator_get_int(ctx, 1, -1) != -1)
				perform_cmd(cmd_goto_doc_percentage, sci, ctx, state);
			else
				perform_cmd(cmd_goto_matching_brace, sci, ctx, state);
			break;
		case GDK_KEY_H:
			perform_cmd(cmd_goto_screen_top, sci, ctx, state);
			break;
		case GDK_KEY_M:
			perform_cmd(cmd_goto_screen_middle, sci, ctx, state);
			break;
		case GDK_KEY_L:
			perform_cmd(cmd_goto_screen_bottom, sci, ctx, state);
			break;
		case GDK_KEY_asciitilde:
			perform_cmd(cmd_uppercase_char, sci, ctx, state);
			break;
		case GDK_KEY_less:
			if (accumulator_previous_char(ctx) == '<')
				perform_cmd_2(cmd_unindent, sci, ctx, state);
			break;
		case GDK_KEY_greater:
			if (accumulator_previous_char(ctx) == '>')
				perform_cmd_2(cmd_indent, sci, ctx, state);
			break;
		//tx, Tx - like above but stop one character before
		default:
			break;
	}
}
