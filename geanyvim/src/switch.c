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


static void perform_cmd(void (*func)(ScintillaObject *sci, ViState *vi_state), ScintillaObject *sci, ViState *vi_state)
{
	func(sci, vi_state);
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
		{
			guint accum_len = accumulator_len(vi_state);
			if (accum_len == 0)
				;
			else if (accum_len == 1 && vi_state->accumulator[0] == 'y')
				perform_cmd(cmd_copy_line, sci, vi_state);
			break;
		}
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
			perform_search(sci, vi_state, TRUE);
			accumulator_clear(vi_state);
			break;
		case GDK_KEY_N:
			perform_search(sci, vi_state, FALSE);
			accumulator_clear(vi_state);
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
			perform_search(sci, vi_state, TRUE);
			accumulator_clear(vi_state);
			break;
		}
		case GDK_KEY_d:
		{
			guint accum_len = accumulator_len(vi_state);
			if (accum_len == 0)
				;
			else if (accum_len == 1 && vi_state->accumulator[0] == 'd')
			{
				gint start = sci_get_position_from_line(sci, sci_get_current_line(sci));
				gint end = sci_get_position_from_line(sci, sci_get_current_line(sci)+1);
				SSM(sci, SCI_DELETERANGE, start, end-start);
				accumulator_clear(vi_state);
			}
			else
				accumulator_clear(vi_state);
			break;
		}

		
		case GDK_KEY_x:
			//delete character
			//SSM(sci, SCI_DELETEBACKNOTLINE, 0, 0);
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
