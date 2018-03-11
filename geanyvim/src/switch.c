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


static void perform_cmd_generic(void (*func)(ScintillaObject *sci, ViState *vi_state, int num), ScintillaObject *sci, ViState *vi_state, gint cmd_len)
{
	gint num = accumulator_get_int(vi_state, cmd_len, 1);
	func(sci, vi_state, num);
	accumulator_clear(vi_state);
}

static void perform_cmd(void (*func)(ScintillaObject *sci, ViState *vi_state, int num), ScintillaObject *sci, ViState *vi_state)
{
	perform_cmd_generic(func, sci, vi_state, 1);
}

static void perform_cmd_2(void (*func)(ScintillaObject *sci, ViState *vi_state, int num), ScintillaObject *sci, ViState *vi_state)
{
	perform_cmd_generic(func, sci, vi_state, 2);
}

static void perform_ui_cmd(void (*func)(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui), ScintillaObject *sci, ViState *state, ViUi *ui)
{
	func(sci, state, ui);
	accumulator_clear(state);
}

void cmd_switch(GdkEventKey *event, ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui)
{
	switch (event->keyval)
	{
		case GDK_KEY_Page_Up:
			perform_cmd(cmd_page_up, sci, vi_state);
			break;
		case GDK_KEY_Page_Down:
			perform_cmd(cmd_page_down, sci, vi_state);
			break;
		case GDK_KEY_Left:
		case GDK_KEY_leftarrow:
		case GDK_KEY_h:
			perform_cmd(cmd_move_caret_left, sci, vi_state);
			break;
		case GDK_KEY_Right:
		case GDK_KEY_rightarrow:
		case GDK_KEY_l:
			perform_cmd(cmd_move_caret_right, sci, vi_state);
			break;
		case GDK_KEY_Down:
		case GDK_KEY_downarrow:
		case GDK_KEY_j:
			perform_cmd(cmd_move_caret_down, sci, vi_state);
			break;
		case GDK_KEY_Up:
		case GDK_KEY_uparrow:
		case GDK_KEY_k:
			perform_cmd(cmd_move_caret_up, sci, vi_state);
			break;
		case GDK_KEY_y:
			if (accumulator_previous_char(vi_state) == 'y')
				perform_cmd_2(cmd_copy_line, sci, vi_state);
			break;
		case GDK_KEY_p:
			perform_cmd(cmd_paste, sci, vi_state);
			break;
		case GDK_KEY_U: // undo on single line - we probably won't implement it
		case GDK_KEY_u:
			perform_cmd(cmd_undo, sci, vi_state);
			break;
		case GDK_KEY_r:
			if (event->state & GDK_CONTROL_MASK)
				perform_cmd(cmd_redo, sci, vi_state);
			break;
		case GDK_KEY_n:
		case GDK_KEY_N:
			perform_cmd(cmd_search, sci, vi_state);
			break;
		case GDK_KEY_asterisk:
		case GDK_KEY_numbersign:
		{
			gchar *word = get_current_word(sci);
			g_free(vi_state->search_text);
			if (!word)
				vi_state->search_text = NULL;
			else
			{
				const gchar *prefix = event->keyval == GDK_KEY_asterisk ? "/" : "?";
				vi_state->search_text = g_strconcat(prefix, word, NULL);
			}
			g_free(word);
			perform_cmd(cmd_search, sci, vi_state);
			break;
		}
		case GDK_KEY_d:
			if (accumulator_previous_char(vi_state) == 'd')
				perform_cmd_2(cmd_delete_line, sci, vi_state);
			break;
		case GDK_KEY_x:
			perform_cmd(cmd_delete_char, sci, vi_state);
			break;
		case GDK_KEY_G:
			perform_cmd(cmd_goto_line_last, sci, vi_state);
			break;
		case GDK_KEY_g:
			if (accumulator_previous_char(vi_state) == 'g')
				perform_cmd_2(cmd_goto_line, sci, vi_state);
			break;
		case GDK_KEY_J:
			perform_cmd(cmd_join_lines, sci, vi_state);
			break;
		case GDK_KEY_w:
		case GDK_KEY_W:
			perform_cmd(cmd_goto_next_word, sci, vi_state);
			break;
		case GDK_KEY_e:
		case GDK_KEY_E:
			perform_cmd(cmd_goto_next_word_end, sci, vi_state);
			break;
		case GDK_KEY_b:
			perform_cmd(cmd_goto_previous_word, sci, vi_state);
			break;
		case GDK_KEY_B:
			perform_cmd(cmd_goto_previous_word_end, sci, vi_state);
			break;
		case GDK_KEY_0:
			if (!isdigit(accumulator_previous_char(vi_state)))
				perform_cmd(cmd_goto_line_start, sci, vi_state);
			break;
		case GDK_KEY_dollar:
			perform_cmd(cmd_goto_line_end, sci, vi_state);
			break;
		case GDK_KEY_percent:
			if (accumulator_len(vi_state) > 1 && accumulator_get_int(vi_state, 1, -1) != -1)
				perform_cmd(cmd_goto_doc_percentage, sci, vi_state);
			else
				perform_cmd(cmd_goto_matching_brace, sci, vi_state);
			break;
		case GDK_KEY_H:
			perform_cmd(cmd_goto_screen_top, sci, vi_state);
			break;
		case GDK_KEY_M:
			perform_cmd(cmd_goto_screen_middle, sci, vi_state);
			break;
		case GDK_KEY_L:
			perform_cmd(cmd_goto_screen_bottom, sci, vi_state);
			break;
		case GDK_KEY_colon:
		case GDK_KEY_slash:
		case GDK_KEY_question:
			perform_ui_cmd(ui_cmd_enter_cmdline_mode, sci, vi_state, vi_ui);
			break;
		case GDK_KEY_i:
			perform_ui_cmd(ui_cmd_enter_insert_mode, sci, vi_state, vi_ui);
			break;
		case GDK_KEY_a:
			perform_ui_cmd(ui_cmd_enter_insert_mode_after, sci, vi_state, vi_ui);
			break;
		case GDK_KEY_I:
			perform_ui_cmd(ui_cmd_enter_insert_mode_line_start, sci, vi_state, vi_ui);
			break;
		case GDK_KEY_A:
			perform_ui_cmd(ui_cmd_enter_insert_mode_line_end, sci, vi_state, vi_ui);
			break;
		case GDK_KEY_Escape:
			accumulator_clear(vi_state);
			break;
		case GDK_KEY_o:
			//new line after current and switch to insert mode
			break;
		case GDK_KEY_O:
			//new line before current
			break;
		//tx, Tx - like above but stop one character before
		default:
			break;
	}
}
