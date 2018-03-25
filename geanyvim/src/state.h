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

#ifndef __GEANYVIM_STATE_H__
#define __GEANYVIM_STATE_H__

#define IS_VISUAL(m) ((m) == VI_MODE_VISUAL || (m) == VI_MODE_VISUAL_LINE || (m) == VI_MODE_VISUAL_BLOCK)
#define IS_COMMAND(m) ((m) == VI_MODE_COMMAND || (m) == VI_MODE_COMMAND_SINGLE)

typedef enum {
	VI_MODE_COMMAND,
	VI_MODE_COMMAND_SINGLE, //performing single command from insert mode using Ctrl+O
	VI_MODE_VISUAL,
	VI_MODE_VISUAL_LINE,
	VI_MODE_VISUAL_BLOCK, //not implemented
	VI_MODE_INSERT,
	VI_MODE_REPLACE,
} ViMode;


void enter_cmdline_mode(void);

void set_vi_mode(ViMode mode);
ViMode get_vi_mode(void);

const gchar *get_inserted_text(void);

#endif
