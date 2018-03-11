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


/* utils */

ScintillaObject *get_current_doc_sci(void)
{
	GeanyDocument *doc = document_get_current();
	return doc != NULL ? doc->editor->sci : NULL;
}

void accumulator_append(ViState *vi_state, const gchar *val)
{
	if (!vi_state->accumulator)
		vi_state->accumulator = g_strdup(val);
	else
		SETPTR(vi_state->accumulator, g_strconcat(vi_state->accumulator, val, NULL));
}

void accumulator_clear(ViState *vi_state)
{
	g_free(vi_state->accumulator);
	vi_state->accumulator = NULL;
}

guint accumulator_len(ViState *vi_state)
{
	if (!vi_state->accumulator)
		return 0;
	return strlen(vi_state->accumulator);
}

gchar accumulator_current_char(ViState *vi_state)
{
	guint len = accumulator_len(vi_state);
	if (len > 0)
		return vi_state->accumulator[len-1];
	return '\0';
}

gchar accumulator_previous_char(ViState *vi_state)
{
	guint len = accumulator_len(vi_state);
	if (len > 1)
		return vi_state->accumulator[len-2];
	return '\0';
}

gint accumulator_get_int(ViState *vi_state, gint start_pos, gint default_val)
{
	gchar *s = g_strdup(vi_state->accumulator);
	gint end = accumulator_len(vi_state) - start_pos - 1;
	gint start = end + 1;
	gint val, i;

	for (i = end; i >= 0; i--)
	{
		if (!isdigit(s[i]))
			break;
		start = i;
	}

	if (end - start < 0)
		return default_val;

	s[end + 1] = '\0';
	val = g_ascii_strtoll(s + start, NULL, 10);

	g_free(s);
	return val;
}


void clamp_cursor_pos(ScintillaObject *sci, ViState *vi_state)
{
	if (vi_state->vi_mode != VI_MODE_COMMAND)
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

void cmd_page_up(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_PAGEUP);
}

void cmd_page_down(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_PAGEDOWN);
}

void cmd_move_caret_left(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint pos = sci_get_current_position(sci);
	gint start_pos = sci_get_position_from_line(sci, sci_get_current_line(sci));
	gint i;
	for (i = 0; i < num && pos > start_pos; i++)
		sci_send_command(sci, SCI_CHARLEFT);
}

void cmd_move_caret_right(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint pos = sci_get_current_position(sci);
	gint end_pos = sci_get_line_end_position(sci, sci_get_current_line(sci));
	gint i;
	for (i = 0; i < num && pos < end_pos - 1; i++)
		sci_send_command(sci, SCI_CHARRIGHT);
}

void cmd_move_caret_up(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_LINEUP);
	clamp_cursor_pos(sci, vi_state);
}

void cmd_move_caret_down(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_LINEDOWN);
	clamp_cursor_pos(sci, vi_state);
}

void cmd_undo(ScintillaObject *sci, ViState *vi_state, gint num)
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

void cmd_redo(ScintillaObject *sci, ViState *vi_state, gint num)
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

void cmd_copy_line(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint start = sci_get_position_from_line(sci, sci_get_current_line(sci));
	gint end = sci_get_position_from_line(sci, sci_get_current_line(sci) + num);
	SSM(sci, SCI_COPYRANGE, start, end);
}

void cmd_paste(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint pos = sci_get_position_from_line(sci, sci_get_current_line(sci)+1);
	gint i;
	sci_set_current_position(sci, pos, TRUE);
	for (i = 0; i < num; i++)
		SSM(sci, SCI_PASTE, 0, 0);
	sci_set_current_position(sci, pos, TRUE);
}

void cmd_delete_line(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint start = sci_get_position_from_line(sci, sci_get_current_line(sci));
	gint end = sci_get_position_from_line(sci, sci_get_current_line(sci) + num);
	SSM(sci, SCI_DELETERANGE, start, end-start);
}

void cmd_search(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gboolean forward = TRUE;
	char last;
	gint i;

	if (!vi_state->accumulator)
		return;

	last = accumulator_current_char(vi_state);

	if (last == 'N')
		forward = FALSE;

	for (i = 0; i < num; i++)
		perform_search(sci, vi_state, forward);
}

void cmd_delete_char(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint pos = sci_get_current_position(sci);
	gint i;
	for (i = 0; i < num; i++)
		SSM(sci, SCI_DELETERANGE, pos, 1);
}

void cmd_goto_line(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint line_num = sci_get_line_count(sci);
	gint pos;

	num = num > line_num ? line_num : num;
	pos = sci_get_position_from_line(sci, num - 1);
	sci_set_current_position(sci, pos, TRUE);
}

void cmd_goto_line_last(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint line_num = sci_get_line_count(sci);

	/* override default line number to the end of file when number not entered */
	num = accumulator_get_int(vi_state, 1, line_num);
	cmd_goto_line(sci, vi_state, num);
}

void cmd_join_lines(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint line = sci_get_current_line(sci);
	gint next_line_pos = sci_get_position_from_line(sci, line+num);

	sci_set_target_start(sci, sci_get_current_position(sci));
	sci_set_target_end(sci, next_line_pos);
	SSM(sci, SCI_LINESJOIN, 0, 0);
}

void cmd_goto_next_word(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_WORDRIGHT);
}

void cmd_goto_next_word_end(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_WORDRIGHTEND);
}

void cmd_goto_previous_word(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_WORDLEFT);
}

void cmd_goto_previous_word_end(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
		sci_send_command(sci, SCI_WORDLEFTEND);
}

void cmd_goto_line_start(ScintillaObject *sci, ViState *vi_state, gint num)
{
	sci_send_command(sci, SCI_HOME);
}

void cmd_goto_line_end(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint i;
	for (i = 0; i < num; i++)
	{
		sci_send_command(sci, SCI_LINEEND);
		if (i != num - 1)
			sci_set_current_position(sci, sci_get_current_position(sci) + 1, TRUE);
	}
}

void cmd_goto_matching_brace(ScintillaObject *sci, ViState *vi_state, gint num)
{
	gint pos = sci_get_current_position(sci);
	pos = SSM(sci, SCI_BRACEMATCH, pos, 0);
	if (pos != -1)
		sci_set_current_position(sci, pos, TRUE);
}
