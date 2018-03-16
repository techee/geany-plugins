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
#include <gdk/gdkkeysyms.h>
#include <ctype.h>

#include "state.h"
#include "cmds.h"
#include "utils.h"


typedef struct
{
	/* current Scintilla object */
	ScintillaObject *sci;
	/* number preceding command - defaults to 1 when not present */
	gint num;
	/* determines if the command was preceded by number */
	gboolean num_present;
	/* last key press */
	KeyPress *last_kp;
	/* current position in scintilla */
	gint pos;
	/* selection length or selection made by movement command */
	gint sel_len;
	/* current line in scintilla */
	gint line;
} CmdParams;


typedef void (*Cmd)(CmdContext *c, CmdParams *p);


static void cmd_mode_cmdline(CmdContext *c, CmdParams *p)
{
	set_vi_mode(VI_MODE_CMDLINE, p->sci);
}

static void cmd_mode_insert(CmdContext *c, CmdParams *p)
{
	set_vi_mode(VI_MODE_INSERT, p->sci);
}

static void cmd_mode_replace(CmdContext *c, CmdParams *p)
{
	set_vi_mode(VI_MODE_REPLACE, p->sci);
}

static void cmd_mode_visual(CmdContext *c, CmdParams *p)
{
	set_vi_mode(VI_MODE_VISUAL, p->sci);
}

static void cmd_mode_insert_after(CmdContext *c, CmdParams *p)
{
	gint end_pos = sci_get_line_end_position(p->sci, p->line);
	if (p->pos < end_pos)
		SSM(p->sci, SCI_CHARRIGHT, 0, 0);

	cmd_mode_insert(c, p);
}

static void cmd_mode_insert_line_start(CmdContext *c, CmdParams *p)
{
	gint pos = p->pos;
	SSM(p->sci, SCI_HOME, 0, 0);
	while (isspace(sci_get_char_at(p->sci, pos)))
	{
		if (sci_get_line_from_position(p->sci, pos + 1) != p->line)
			break;
		pos++;
	}
	sci_set_current_position(p->sci, pos, TRUE);
	cmd_mode_insert(c, p);
}

static void cmd_mode_insert_line_end(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_LINEEND, 0, 0);
	cmd_mode_insert(c, p);
}

static void cmd_mode_insert_next_line(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_LINEEND, 0, 0);
	SSM(p->sci, SCI_NEWLINE, 0, 0);
	cmd_mode_insert(c, p);
}

static void cmd_mode_insert_prev_line(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_HOME, 0, 0);
	SSM(p->sci, SCI_NEWLINE, 0, 0);
	SSM(p->sci, SCI_LINEUP, 0, 0);
	cmd_mode_insert(c, p);
}

static void cmd_mode_insert_clear_line(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_DELLINELEFT, 0, 0);
	SSM(p->sci, SCI_DELLINERIGHT, 0, 0);
	cmd_mode_insert(c, p);
}

static void cmd_mode_insert_clear_right(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_DELLINERIGHT, 0, 0);
	cmd_mode_insert(c, p);
}

static void cmd_mode_insert_delete_char(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_DELETERANGE, p->pos, 1);
	cmd_mode_insert(c, p);
}

static void cmd_goto_page_up(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_PAGEUP, 0, 0);
}

static void cmd_goto_page_down(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_PAGEDOWN, 0, 0);
}

static void cmd_goto_left(CmdContext *c, CmdParams *p)
{
	gint i;
	gint start_pos = sci_get_position_from_line(p->sci, p->line);
	for (i = 0; i < p->num && p->pos > start_pos; i++)
		SSM(p->sci, SCI_CHARLEFT, 0, 0);
}

static void cmd_goto_right(CmdContext *c, CmdParams *p)
{
	gint i;
	gint end_pos = sci_get_line_end_position(p->sci, p->line);
	for (i = 0; i < p->num && p->pos < end_pos - 1; i++)
		SSM(p->sci, SCI_CHARRIGHT, 0, 0);
}

static void cmd_goto_up(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_LINEUP, 0, 0);
}

static void cmd_goto_down(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_LINEDOWN, 0, 0);
}

static void cmd_undo(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
	{
		if (SSM(p->sci, SCI_CANUNDO, 0, 0))
			SSM(p->sci, SCI_UNDO, 0, 0);
		else
			break;
	}
}

static void cmd_redo(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
	{
		if (SSM(p->sci, SCI_CANREDO, 0, 0))
			SSM(p->sci, SCI_REDO, 0, 0);
		else
			break;
	}
}

static void cmd_copy_line(CmdContext *c, CmdParams *p)
{
	gint start = sci_get_position_from_line(p->sci, p->line);
	gint end = sci_get_position_from_line(p->sci, p->line + p->num);
	SSM(p->sci, SCI_COPYRANGE, start, end);
}

static void cmd_paste(CmdContext *c, CmdParams *p)
{
	gint i;
	gint pos = sci_get_position_from_line(p->sci, p->line+1);
	sci_set_current_position(p->sci, pos, TRUE);
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_PASTE, 0, 0);
	sci_set_current_position(p->sci, pos, TRUE);
}

static void cmd_delete_line(CmdContext *c, CmdParams *p)
{
	gint start = sci_get_position_from_line(p->sci, p->line);
	gint end = sci_get_position_from_line(p->sci, p->line + p->num);
	SSM(p->sci, SCI_DELETERANGE, start, end-start);
}

static void cmd_search(CmdContext *c, CmdParams *p)
{
	gboolean forward = TRUE;
	gint i;

	if (p->last_kp->key == GDK_KEY_N)
		forward = FALSE;

	for (i = 0; i < p->num; i++)
		perform_search(p->sci, c, forward);
}

static void search_current(CmdContext *c, CmdParams *p, gboolean next)
{
	gchar *word = get_current_word(p->sci);
	g_free(c->search_text);
	if (!word)
		c->search_text = NULL;
	else
	{
		const gchar *prefix = next ? "/" : "?";
		c->search_text = g_strconcat(prefix, word, NULL);
	}
	g_free(word);
	cmd_search(c, p);
}

static void cmd_search_current_next(CmdContext *c, CmdParams *p)
{
	search_current(c, p, TRUE);
}

static void cmd_search_current_prev(CmdContext *c, CmdParams *p)
{
	search_current(c, p, FALSE);
}

static void cmd_delete_char(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_DELETERANGE, p->pos, 1);
}

static void cmd_delete_char_back(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_DELETERANGE, p->pos-1, 1);
}

static void cmd_goto_line(CmdContext *c, CmdParams *p)
{
	gint line_num = sci_get_line_count(p->sci);
	gint num = p->num > line_num ? line_num : p->num;
	gint pos;
	
	pos = sci_get_position_from_line(p->sci, num - 1);
	sci_set_current_position(p->sci, pos, TRUE);
}

static void cmd_goto_line_last(CmdContext *c, CmdParams *p)
{
	gint line_num = sci_get_line_count(p->sci);
	p->num = p->num_present ? p->num : line_num;
	cmd_goto_line(c, p);
}

static void cmd_join_lines(CmdContext *c, CmdParams *p)
{
	gint next_line_pos = sci_get_position_from_line(p->sci, p->line + p->num);

	sci_set_target_start(p->sci, p->pos);
	sci_set_target_end(p->sci, next_line_pos);
	SSM(p->sci, SCI_LINESJOIN, 0, 0);
}

static void cmd_goto_next_word(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_WORDRIGHT, 0, 0);
}

static void cmd_goto_next_word_end(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_WORDRIGHTEND, 0, 0);
}

static void cmd_goto_previous_word(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_WORDLEFT, 0, 0);
}

static void cmd_goto_previous_word_end(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_WORDLEFTEND, 0, 0);
}

static void cmd_goto_line_start(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_HOME, 0, 0);
}

static void cmd_goto_line_end(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
	{
		SSM(p->sci, SCI_LINEEND, 0, 0);
		if (i != p->num - 1)
			sci_set_current_position(p->sci, sci_get_current_position(p->sci) + 1, TRUE);
	}
}

static void cmd_goto_matching_brace(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_BRACEMATCH, p->pos, 0);
	if (pos != -1)
		sci_set_current_position(p->sci, pos, TRUE);
}

static void cmd_goto_doc_percentage(CmdContext *c, CmdParams *p)
{
	gint line_num = sci_get_line_count(p->sci);
	gint pos;

	if (p->num > 100)
		p->num = 100;

	line_num = (line_num * p->num) / 100;
	pos = sci_get_position_from_line(p->sci, line_num);
	sci_set_current_position(p->sci, pos, TRUE);
}

static void cmd_goto_screen_top(CmdContext *c, CmdParams *p)
{
	gint top = SSM(p->sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	gint pos = sci_get_position_from_line(p->sci, top+p->num-1);
	sci_set_current_position(p->sci, pos, TRUE);
}

static void cmd_goto_screen_middle(CmdContext *c, CmdParams *p)
{
	gint top = SSM(p->sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	gint count = SSM(p->sci, SCI_LINESONSCREEN, 0, 0);
	gint pos = sci_get_position_from_line(p->sci, top+count/2);
	sci_set_current_position(p->sci, pos, TRUE);
}

static void cmd_goto_screen_bottom(CmdContext *c, CmdParams *p)
{
	gint top = SSM(p->sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	gint count = SSM(p->sci, SCI_LINESONSCREEN, 0, 0);
	gint pos = sci_get_position_from_line(p->sci, top+count-p->num);
	sci_set_current_position(p->sci, pos, TRUE);
}

static void cmd_replace_char(CmdContext *c, CmdParams *p)
{
	gchar repl[2] = {kp_to_char(p->last_kp), '\0'};

	sci_set_target_start(p->sci, p->pos);
	sci_set_target_end(p->sci, p->pos + 1);
	sci_replace_target(p->sci, repl, FALSE);
}

static void cmd_uppercase_char(CmdContext *c, CmdParams *p)
{
	gchar upper[2] = {toupper(sci_get_char_at(p->sci, p->pos)), '\0'};

	sci_set_target_start(p->sci, p->pos);
	sci_set_target_end(p->sci, p->pos + 1);
	sci_replace_target(p->sci, upper, FALSE);
	SSM(p->sci, SCI_CHARRIGHT, 0, 0);
}

static void cmd_indent(CmdContext *c, CmdParams *p)
{
	gint i;
	gint pos = p->pos;

	for (i = 0; i < p->num; i++)
	{
		SSM(p->sci, SCI_HOME, 0, 0);
		SSM(p->sci, SCI_TAB, 0, 0);
		if (i == 0)
			pos = sci_get_current_position(p->sci);
		SSM(p->sci, SCI_LINEDOWN, 0, 0);
	}
	sci_set_current_position(p->sci, pos, FALSE);
}

static void cmd_unindent(CmdContext *c, CmdParams *p)
{
	gint i;
	gint pos = p->pos;

	for (i = 0; i < p->num; i++)
	{
		SSM(p->sci, SCI_HOME, 0, 0);
		SSM(p->sci, SCI_BACKTAB, 0, 0);
		if (i == 0)
			pos = sci_get_current_position(p->sci);
		SSM(p->sci, SCI_LINEDOWN, 0, 0);
	}
	sci_set_current_position(p->sci, pos, FALSE);
}

static void cmd_range_delete(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_DELETERANGE, p->pos, p->sel_len);
}

static void cmd_repeat_last_command(CmdContext *c, CmdParams *p)
{
	// fake
}

/******************************************************************************/


typedef struct {
	Cmd cmd;
	guint key1;
	guint key2;
	guint modif1;
	guint modif2;
	gboolean param;
} CmdDef;


#define MOVEMENT_CMDS \
	{cmd_goto_page_up, GDK_KEY_Page_Up, 0, 0, 0, FALSE}, \
	{cmd_goto_page_down, GDK_KEY_Page_Down, 0, 0, 0, FALSE}, \
	{cmd_goto_left, GDK_KEY_Left, 0, 0, 0, FALSE}, \
	{cmd_goto_left, GDK_KEY_leftarrow, 0, 0, 0, FALSE}, \
	{cmd_goto_left, GDK_KEY_h, 0, 0, 0, FALSE}, \
	{cmd_goto_right, GDK_KEY_Right, 0, 0, 0, FALSE}, \
	{cmd_goto_right, GDK_KEY_rightarrow, 0, 0, 0, FALSE}, \
	{cmd_goto_right, GDK_KEY_l, 0, 0, 0, FALSE}, \
	{cmd_goto_down, GDK_KEY_Down, 0, 0, 0, FALSE}, \
	{cmd_goto_down, GDK_KEY_downarrow, 0, 0, 0, FALSE}, \
	{cmd_goto_down, GDK_KEY_j, 0, 0, 0, FALSE}, \
	{cmd_goto_up, GDK_KEY_Up, 0, 0, 0, FALSE}, \
	{cmd_goto_up, GDK_KEY_uparrow, 0, 0, 0, FALSE}, \
	{cmd_goto_up, GDK_KEY_k, 0, 0, 0, FALSE}, \
	{cmd_goto_line_last, GDK_KEY_G, 0, 0, 0, FALSE}, \
	{cmd_goto_line, GDK_KEY_g, GDK_KEY_g, 0, 0, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_w, 0, 0, 0, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_W, 0, 0, 0, FALSE}, \
	{cmd_goto_next_word_end, GDK_KEY_e, 0, 0, 0, FALSE}, \
	{cmd_goto_next_word_end, GDK_KEY_E, 0, 0, 0, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_b, 0, 0, 0, FALSE}, \
	{cmd_goto_previous_word_end, GDK_KEY_B, 0, 0, 0, FALSE}, \
	{cmd_goto_line_end, GDK_KEY_dollar, 0, 0, 0, FALSE}, \
	{cmd_goto_line_start, GDK_KEY_0, 0, 0, 0, FALSE}, \
	{cmd_goto_screen_top, GDK_KEY_H, 0, 0, 0, FALSE}, \
	{cmd_goto_screen_middle, GDK_KEY_M, 0, 0, 0, FALSE}, \
	{cmd_goto_screen_bottom, GDK_KEY_L, 0, 0, 0, FALSE}, \
	{cmd_goto_doc_percentage, GDK_KEY_percent, 0, 0, 0, FALSE}, \
	{cmd_goto_matching_brace, GDK_KEY_percent, 0, 0, 0, FALSE},


CmdDef movement_cmds[] = {
	MOVEMENT_CMDS
	{NULL, 0, 0, 0, 0, FALSE}
};


CmdDef cmd_mode_cmds[] = {
	/* enter cmdline mode */
	{cmd_mode_cmdline, GDK_KEY_colon, 0, 0, 0, FALSE},
	{cmd_mode_cmdline, GDK_KEY_slash, 0, 0, 0, FALSE},
	{cmd_mode_cmdline, GDK_KEY_question, 0, 0, 0, FALSE},
	/* enter insert mode */
	{cmd_mode_insert, GDK_KEY_i, 0, 0, 0, FALSE},
	{cmd_mode_insert_after, GDK_KEY_a, 0, 0, 0, FALSE},
	{cmd_mode_insert_line_start, GDK_KEY_I, 0, 0, 0, FALSE},
	{cmd_mode_insert_line_end, GDK_KEY_A, 0, 0, 0, FALSE},//not correct pos
	{cmd_mode_insert_next_line, GDK_KEY_o, 0, 0, 0, FALSE},
	{cmd_mode_insert_clear_line, GDK_KEY_S, 0, 0, 0, FALSE},
	{cmd_mode_insert_clear_line, GDK_KEY_c, GDK_KEY_c, 0, 0, FALSE},
	{cmd_mode_insert_prev_line, GDK_KEY_O, 0, 0, 0, FALSE},
	{cmd_mode_insert_clear_right, GDK_KEY_C, 0, 0, 0, FALSE},
	{cmd_mode_insert_delete_char, GDK_KEY_s, 0, 0, 0, FALSE},
	/* enter replace mode */
	{cmd_mode_replace, GDK_KEY_R, 0, 0, 0, FALSE},
	/* enter visual mode */
	{cmd_mode_visual, GDK_KEY_v, 0, 0, 0, FALSE},

	/* copy/paste */
	{cmd_copy_line, GDK_KEY_y, GDK_KEY_y, 0, 0, FALSE},
	{cmd_paste, GDK_KEY_p, 0, 0, 0, FALSE},

	/* undo/redo */
	{cmd_undo, GDK_KEY_U, 0, 0, 0, FALSE},
	{cmd_undo, GDK_KEY_u, 0, 0, 0, FALSE},
	{cmd_redo, GDK_KEY_r, 0, GDK_CONTROL_MASK, 0, FALSE},

	/* search */
	{cmd_search, GDK_KEY_n, 0, 0, 0, FALSE},
	{cmd_search, GDK_KEY_N, 0, 0, 0, FALSE},
	{cmd_search_current_next, GDK_KEY_asterisk, 0, 0, 0, FALSE},
	{cmd_search_current_prev, GDK_KEY_numbersign, 0, 0, 0, FALSE},

	/* deletion */
	{cmd_delete_line, GDK_KEY_D, 0, 0, 0, FALSE},
	{cmd_delete_line, GDK_KEY_d, GDK_KEY_d, 0, 0, FALSE},
	{cmd_delete_char, GDK_KEY_x, 0, 0, 0, FALSE},
	{cmd_delete_char_back, GDK_KEY_X, 0, 0, 0, FALSE},

	/* modification */
	{cmd_join_lines, GDK_KEY_J, 0, 0, 0, FALSE},
	{cmd_uppercase_char, GDK_KEY_asciitilde, 0, 0, 0, FALSE},//not correct position when undoing
	{cmd_unindent, GDK_KEY_less, GDK_KEY_less, 0, 0, FALSE},
	{cmd_indent, GDK_KEY_greater, GDK_KEY_greater, 0, 0, FALSE},
	{cmd_replace_char, GDK_KEY_r, 0, 0, 0, TRUE},

	/* special */
	{cmd_repeat_last_command, GDK_KEY_period, 0, 0, 0, FALSE},

	MOVEMENT_CMDS

	{NULL, 0, 0, 0, 0, FALSE}
};


CmdDef range_cmds[] = {
	{cmd_range_delete, GDK_KEY_d, 0, 0, 0, FALSE},
	{NULL, 0, 0, 0, 0, FALSE}
};


static gboolean is_in_cmd_group(CmdDef *cmds, CmdDef *def)
{
	int i;
	for (i = 0; cmds[i].cmd != NULL; i++)
	{
		CmdDef *d = &cmds[i];
		if (def->cmd == d->cmd && def->key1 == d->key1 && def->key2 == d->key2 &&
			def->modif1 == d->modif1 && def->modif2 == d->modif2 && def->param == d->param)
			return TRUE;
	}
	return FALSE;
}

static gboolean key_equals(KeyPress *kp, guint key, guint modif)
{
	return kp->key == key && (kp->modif & modif || kp->modif == modif);
}


static CmdDef *get_cmd_to_run(GSList *kpl, CmdDef *cmds)
{
	gint i;
	KeyPress *curr = g_slist_nth_data(kpl, 0);
	KeyPress *prev = g_slist_nth_data(kpl, 1);

	if (!kpl)
		return NULL;

	// commands such as rc or fc (replace char c, find char c) which are specified
	// by the previous character and current character is used as their parameter
	if (prev != NULL && !kp_isdigit(prev))
	{
		for (i = 0; cmds[i].cmd != NULL; i++)
		{
			CmdDef *cmd = &cmds[i];
			if (cmd->key2 == 0 && cmd->param &&
					key_equals(prev, cmd->key1, cmd->modif1))
				return cmd;
		}
	}

	// 2-letter commands
	if (prev != NULL && !kp_isdigit(prev))
	{
		for (i = 0; cmds[i].cmd != NULL; i++)
		{
			CmdDef *cmd = &cmds[i];
			if (cmd->key2 != 0 && !cmd->param &&
					key_equals(curr, cmd->key2, cmd->modif2) &&
					key_equals(prev, cmd->key1, cmd->modif1))
				return cmd;
		}
	}

	// 1-letter commands
	for (i = 0; cmds[i].cmd != NULL; i++)
	{
		CmdDef *cmd = &cmds[i];
		if (cmd->key2 == 0 && !cmd->param &&
			key_equals(curr, cmd->key1, cmd->modif1))
		{
			// now solve some quirks manually
			if (curr->key == GDK_KEY_0)
			{
				// 0 jumps to the beginning of line only when not preceded
				// by another number in which case we want to add it to the accumulator
				if (prev == NULL || !kp_isdigit(prev))
					return cmd;
			}
			else if (curr->key == GDK_KEY_percent)
			{
				Cmd c = cmd_goto_matching_brace;
				gint val = kpl_get_int(kpl, NULL);
				if (val != -1)
					c = cmd_goto_doc_percentage;
				if (cmd->cmd == c)
					return cmd;
			}
			else
				return cmd;
		}
	}

	return NULL;
}


static void perform_cmd(CmdDef *def, ScintillaObject *sci, CmdContext *ctx, GSList *kpl)
{
	GSList *top = g_slist_nth(kpl, def->key2 != 0 ? 2 : 1);
	gint num = kpl_get_int(top, &top);
	gboolean num_present = num != -1;
	CmdParams param;
	gint orig_pos;

	param.sci = sci;
	param.num = num_present ? num : 1;
	param.num_present = num_present;
	param.last_kp = g_slist_nth_data(kpl, 0);
	param.pos = sci_get_current_position(sci);
	param.sel_len = 0;
	param.line = sci_get_current_line(sci);

	sci_start_undo_action(sci);

	orig_pos = sci_get_current_position(sci);
	def->cmd(ctx, &param);

	if (is_in_cmd_group(movement_cmds, def))
	{
		def = get_cmd_to_run(top, range_cmds);
		if (def)
		{
			gint new_pos = sci_get_current_position(sci);

			sci_set_current_position(sci, orig_pos, FALSE);

			param.num = 1;
			param.num_present = FALSE;
			param.last_kp = g_slist_nth_data(top, 0);
			param.pos = MIN(new_pos, orig_pos);
			param.sel_len = ABS(new_pos - orig_pos);
			def->cmd(ctx, &param);
		}
	}

	clamp_cursor_pos(sci);
	sci_end_undo_action(sci);
}


gboolean process_event_cmd_mode(ScintillaObject *sci, CmdContext *ctx, GSList *kpl, GSList *prev_kpl, gboolean *is_repeat)
{
	CmdDef *def = get_cmd_to_run(kpl, cmd_mode_cmds);

	*is_repeat = FALSE;
	if (!def)
		return FALSE;

	*is_repeat = def->cmd == cmd_repeat_last_command;
	if (*is_repeat)
	{
		kpl = prev_kpl;
		def = get_cmd_to_run(kpl, cmd_mode_cmds);
		if (!def)
			return FALSE;
	}

	perform_cmd(def, sci, ctx, kpl);

	return TRUE;
}
