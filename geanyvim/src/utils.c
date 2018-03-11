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

#include <ctype.h>

#include "utils.h"

ScintillaObject *get_current_doc_sci(void)
{
	GeanyDocument *doc = document_get_current();
	return doc != NULL ? doc->editor->sci : NULL;
}

void accumulator_append(CmdContext *ctx, const gchar *val)
{
	if (!ctx->accumulator)
		ctx->accumulator = g_strdup(val);
	else
		SETPTR(ctx->accumulator, g_strconcat(ctx->accumulator, val, NULL));
}

void accumulator_clear(CmdContext *ctx)
{
	g_free(ctx->accumulator);
	ctx->accumulator = NULL;
}

guint accumulator_len(CmdContext *ctx)
{
	if (!ctx->accumulator)
		return 0;
	return strlen(ctx->accumulator);
}

gchar accumulator_current_char(CmdContext *ctx)
{
	guint len = accumulator_len(ctx);
	if (len > 0)
		return ctx->accumulator[len-1];
	return '\0';
}

gchar accumulator_previous_char(CmdContext *ctx)
{
	guint len = accumulator_len(ctx);
	if (len > 1)
		return ctx->accumulator[len-2];
	return '\0';
}

gint accumulator_get_int(CmdContext *ctx, gint start_pos, gint default_val)
{
	gchar *s = g_strdup(ctx->accumulator);
	gint end = accumulator_len(ctx) - start_pos - 1;
	gint start = end + 1;
	gint val, i;

	for (i = end; i >= 0; i--)
	{
		if (!isdigit(s[i]))
			break;
		start = i;
	}

	if (end - start < 0)
		val = default_val;
	else
	{
		s[end + 1] = '\0';
		val = g_ascii_strtoll(s + start, NULL, 10);
	}

	g_free(s);
	return val;
}


void clamp_cursor_pos(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	if (!state->vi_enabled || state->vi_mode != VI_MODE_COMMAND)
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

void perform_search(ScintillaObject *sci, CmdContext *ctx, gboolean forward)
{
	struct Sci_TextToFind ttf;
	gint loc, len, pos;

	if (!ctx->search_text)
		return;

	len = sci_get_length(sci);
	pos = sci_get_current_position(sci);

	forward = (ctx->search_text[0] == '/' && forward) ||
			(ctx->search_text[0] == '?' && !forward);
	ttf.lpstrText = ctx->search_text + 1;
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

static const gchar *get_mode_name(ViMode vi_mode)
{
	switch (vi_mode)
	{
		case VI_MODE_COMMAND:
			return "COMMAND";
			break;
		case VI_MODE_INSERT:
			return "INSERT";
			break;
		case VI_MODE_REPLACE:
			return "REPLACE";
			break;
		case VI_MODE_VISUAL:
			return "VISUAL";
			break;
	}
	return "";
}

void prepare_vi_mode(ScintillaObject *sci, CmdContext *ctx, ViState *state)
{
	if (!sci)
		return;

	if (state->default_caret_style == -1)
		state->default_caret_style = SSM(sci, SCI_GETCARETSTYLE, 0, 0);

	if (state->vi_enabled)
	{
		if (state->vi_mode == VI_MODE_COMMAND)
		{
			SSM(sci, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
			SSM(sci, SCI_SETOVERTYPE, 0, 0);
		}
		else
		{
			if (state->vi_mode == VI_MODE_INSERT)
				SSM(sci, SCI_SETOVERTYPE, 0, 0);
			else if (state->vi_mode == VI_MODE_REPLACE)
				SSM(sci, SCI_SETOVERTYPE, 1, 0);
			SSM(sci, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
		}
		ui_set_statusbar(FALSE, "Vim Mode: -- %s --", get_mode_name(state->vi_mode));
	}
	else
		SSM(sci, SCI_SETCARETSTYLE, state->default_caret_style, 0);

	clamp_cursor_pos(sci, ctx, state);
}

