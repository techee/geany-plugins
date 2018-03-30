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

#ifndef __GEANYVIM_VIM_H__
#define __GEANYVIM_VIM_H__

#include <gtk/gtk.h>
#include "Scintilla.h"
#include "ScintillaWidget.h"

#define IS_VISUAL(m) ((m) == VI_MODE_VISUAL || (m) == VI_MODE_VISUAL_LINE || (m) == VI_MODE_VISUAL_BLOCK)
#define IS_COMMAND(m) ((m) == VI_MODE_COMMAND || (m) == VI_MODE_COMMAND_SINGLE)
#define IS_INSERT(m) ((m) == VI_MODE_INSERT || (m) == VI_MODE_REPLACE)

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


gboolean on_sc_notification(SCNotification *nt);
gboolean on_key_press_notification(GdkEventKey *event);

void vim_init(GtkWidget *window);
void vim_cleanup(void);

void vim_sc_init(ScintillaObject *sci);
void vim_sc_cleanup(ScintillaObject *sci);

void set_vim_enabled(gboolean enabled);
void set_start_in_insert(gboolean enabled);
void set_insert_for_dummies(gboolean enabled);

gboolean get_vim_enabled(void);
gboolean get_start_in_insert(void);
gboolean get_insert_for_dummies(void);

#endif
