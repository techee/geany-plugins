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

#include "switch.h"
#include "cmds.h"


static void perform_cmd(void (*func)(ScintillaObject *sci, ViState *vi_state, int num), ScintillaObject *sci, ViState *vi_state)
{
	gint num = accumulator_get_prev_int(vi_state, 1);
	func(sci, vi_state, num);
	accumulator_clear(vi_state);
}

gboolean cmd_switch(GdkEventKey *event, ScintillaObject *sci, ViState *vi_state)
{
	gboolean performed = TRUE;

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
				perform_cmd(cmd_copy_line, sci, vi_state);
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
				perform_cmd(cmd_delete_line, sci, vi_state);
			break;
		case GDK_KEY_x:
			perform_cmd(cmd_delete_char, sci, vi_state);
			break;
		case GDK_KEY_G:
			perform_cmd(cmd_goto_line, sci, vi_state);
			break;
		case GDK_KEY_J:
			//join lines
			break;
		case GDK_KEY_o:
			//new line after current and switch to insert mode
			break;
		case GDK_KEY_O:
			//new line before current
			break;
		case GDK_KEY_w:
			//move to next word
			break;
		case GDK_KEY_b:
			//move to previous word
			break;
		//home, 0 (zero) - move to start of line
		//F - like above backwards
		//tx, Tx - like above but stop one character before
		//% go to matching parenthesis
		//numG - move to line 'num'
		//50% - go to half of the file
		//H, M, L - moving within visible editor area
		default:
			performed = FALSE;
	}

	return performed;
}
