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
		default:
			performed = FALSE;
	}

	return performed;
}
