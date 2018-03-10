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
#include "state.h"
#include "cmds.h"

#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)


/* utils */

void clamp_cursor_pos(ScintillaObject *sci, ViState *vi_state)
{
	if (vi_state->insert_mode)
		return;

	gint pos = sci_get_current_position(sci);
	gint start_pos = sci_get_position_from_line(sci, sci_get_current_line(sci));
	gint end_pos = sci_get_line_end_position(sci, sci_get_current_line(sci));
	if (pos == end_pos && pos != start_pos)
		sci_send_command(sci, SCI_CHARLEFT);
}

gchar *get_current_word(ScintillaObject *sci)
{
	gint start, end, pos;

	if (!sci)
		return NULL;

	pos = sci_get_current_position(sci);
	SSM(sci, SCI_WORDSTARTPOSITION, pos, TRUE);
	start = SSM(sci, SCI_WORDSTARTPOSITION, pos, TRUE);
	end = SSM(sci, SCI_WORDENDPOSITION, pos, TRUE);

	if (start == end)
		return NULL;

	if (end - start >= GEANY_MAX_WORD_LENGTH)
		end = start + GEANY_MAX_WORD_LENGTH - 1;
	return sci_get_contents_range(sci, start, end);
}

void perform_search(ScintillaObject *sci, ViState *vi_state, gboolean forward)
{
	struct Sci_TextToFind ttf;
	gint loc, len, pos;

	if (!vi_state->search_text)
		return;

	len = sci_get_length(sci);
	pos = sci_get_current_position(sci);

	forward = (vi_state->search_text[0] == '/' && forward) ||
			(vi_state->search_text[0] == '?' && !forward);
	ttf.lpstrText = vi_state->search_text + 1;
	if (forward)
	{
		ttf.chrg.cpMin = pos + 1;
		ttf.chrg.cpMax = len;
	}
	else
	{
		ttf.chrg.cpMin = pos - 1;
		ttf.chrg.cpMax = 0;
	}

	loc = sci_find_text(sci, 0, &ttf);
	if (loc < 0)
	{
		/* wrap */
		if (forward)
		{
			ttf.chrg.cpMin = 0;
			ttf.chrg.cpMax = pos;
		}
		else
		{
			ttf.chrg.cpMin = len;
			ttf.chrg.cpMax = pos;
		}

		loc = sci_find_text(sci, 0, &ttf);
	}

	if (loc >= 0)
		sci_set_current_position(sci, loc, TRUE);
}


/* cmds */

void cmd_page_up(ScintillaObject *sci, ViState *vi_state)
{
	sci_send_command(sci, SCI_PAGEUP);
}

void cmd_page_down(ScintillaObject *sci, ViState *vi_state)
{
	sci_send_command(sci, SCI_PAGEDOWN);
}

void cmd_move_caret_left(ScintillaObject *sci, ViState *vi_state)
{
	gint pos = sci_get_current_position(sci);
	gint start_pos = sci_get_position_from_line(sci, sci_get_current_line(sci));
	if (pos > start_pos)
		sci_send_command(sci, SCI_CHARLEFT);
}

void cmd_move_caret_right(ScintillaObject *sci, ViState *vi_state)
{
	gint pos = sci_get_current_position(sci);
	gint end_pos = sci_get_line_end_position(sci, sci_get_current_line(sci));
	if (pos < end_pos - 1)
		sci_send_command(sci, SCI_CHARRIGHT);
}

void cmd_move_caret_up(ScintillaObject *sci, ViState *vi_state)
{
	sci_send_command(sci, SCI_LINEUP);
	clamp_cursor_pos(sci, vi_state);
}

void cmd_move_caret_down(ScintillaObject *sci, ViState *vi_state)
{
	sci_send_command(sci, SCI_LINEDOWN);
	clamp_cursor_pos(sci, vi_state);
}

