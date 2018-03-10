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
#include "ui.h"
#include "ui_cmds.h"
#include "cmds.h"

/* utils */

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
		case VI_MODE_VISUAL:
			return "VISUAL";
			break;
	}
	return "";
}

void prepare_vi_mode(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui)
{
	if (!sci)
		return;

	if (vi_ui->default_caret_style == -1)
		vi_ui->default_caret_style = SSM(sci, SCI_GETCARETSTYLE, 0, 0);

	if (vi_state->vi_enabled)
	{
		if (vi_state->vi_mode == VI_MODE_INSERT)
			SSM(sci, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
		else
			SSM(sci, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
	}
	else
		SSM(sci, SCI_SETCARETSTYLE, vi_ui->default_caret_style, 0);

	if (vi_state->vi_enabled)
		ui_set_statusbar(FALSE, "Vim Mode: -- %s --", get_mode_name(vi_state->vi_mode));

	clamp_cursor_pos(sci, vi_state);
}

/* cmds */

void ui_cmd_enter_cmdline_mode(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui)
{
	const gchar *val = vi_state->accumulator + strlen(vi_state->accumulator) - 1;;
	gtk_widget_show(vi_ui->prompt);
	gtk_entry_set_text(GTK_ENTRY(vi_ui->entry), val);
	gtk_editable_set_position(GTK_EDITABLE(vi_ui->entry), 1);
}

void ui_cmd_enter_insert_mode(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui)
{
	vi_state->vi_mode = VI_MODE_INSERT;
	prepare_vi_mode(sci, vi_state, vi_ui);
}

void ui_cmd_enter_insert_mode_after(ScintillaObject *sci, ViState *vi_state, ViUi *vi_ui)
{
	gint pos = sci_get_current_position(sci);
	gint end_pos = sci_get_line_end_position(sci, sci_get_current_line(sci));
	if (pos < end_pos)
		sci_send_command(sci, SCI_CHARRIGHT);

	ui_cmd_enter_insert_mode(sci, vi_state, vi_ui);
}
