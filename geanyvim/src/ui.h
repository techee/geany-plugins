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

#ifndef __GEANYVIM_UI_H__
#define __GEANYVIM_UI_H__

#include <glib.h>

typedef struct
{
	GtkWidget *prompt;
	GtkWidget *entry;
	GtkWidget *separator_item;
	GtkWidget *toggle_vi_item;
	GtkWidget *perform_vi_item;

	/* caret style used by Geany we can revert to when disabling vi mode */
	gint default_caret_style;
} ViUi;

#endif
