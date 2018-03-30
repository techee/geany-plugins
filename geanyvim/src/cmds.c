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
	/* if running as an "operator" command */
	gboolean is_operator_cmd;

	/* selection start or selection made by movement command */
	gint sel_start;
	/* selection length or selection made by movement command */
	gint sel_len;
	/* first line of selection */
	gint sel_first_line;
	/* the beginning of the first line of selection */
	gint sel_first_line_begin_pos;
	/* last line of selection */
	gint sel_last_line;
	/* the end of the last line of selection */
	gint sel_last_line_end_pos;

	/* current position in scintilla */
	gint pos;
	/* current line in scintilla */
	gint line;
	/* position of the end of the current line */
	gint line_end_pos;
	/* position of the start of the current line */
	gint line_start_pos;
	/* number of lines in document */
	gint line_num;
	/* first visible line */
	gint line_visible_first;
	/* number of displayed lines */
	gint line_visible_num;
} CmdParams;


typedef void (*Cmd)(CmdContext *c, CmdParams *p);

static void init_cmd_params(CmdParams *param, ScintillaObject *sci,
	gint num, gboolean num_present, GSList *kpl, gboolean is_operator_cmd,
	gint sel_start, gint sel_len)
{
	param->sci = sci;

	param->num = num;
	param->num_present = num_present;
	param->last_kp = g_slist_nth_data(kpl, 0);
	param->is_operator_cmd = is_operator_cmd;

	param->sel_start = sel_start;
	param->sel_len = sel_len;
	param->sel_first_line = SSM(sci, SCI_LINEFROMPOSITION, sel_start, 0);
	param->sel_first_line_begin_pos = SSM(sci, SCI_POSITIONFROMLINE, param->sel_first_line, 0);
	param->sel_last_line = SSM(sci, SCI_LINEFROMPOSITION, sel_start + sel_len, 0);
	param->sel_last_line_end_pos = SSM(sci, SCI_GETLINEENDPOSITION, param->sel_last_line, 0);

	param->pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
	param->line = GET_CUR_LINE(sci);
	param->line_end_pos = SSM(sci, SCI_GETLINEENDPOSITION, param->line, 0);
	param->line_start_pos = SSM(sci, SCI_POSITIONFROMLINE, param->line, 0);
	param->line_num = SSM(sci, SCI_GETLINECOUNT, 0, 0);
	param->line_visible_first = SSM(sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	param->line_visible_num = SSM(sci, SCI_LINESONSCREEN, 0, 0);
}


static void cmd_mode_cmdline(CmdContext *c, CmdParams *p)
{
	c->num = p->num;
	enter_cmdline_mode();
}

static void cmd_mode_insert(CmdContext *c, CmdParams *p)
{
	c->num = p->num;
	c->newline_insert = FALSE;
	set_vi_mode(VI_MODE_INSERT);
}

static void cmd_mode_replace(CmdContext *c, CmdParams *p)
{
	c->num = p->num;
	c->newline_insert = FALSE;
	set_vi_mode(VI_MODE_REPLACE);
}

static void cmd_mode_visual(CmdContext *c, CmdParams *p)
{
	if (get_vi_mode() == VI_MODE_VISUAL)
	{
		SSM(p->sci, SCI_SETEMPTYSELECTION, p->pos, 0);
		set_vi_mode(VI_MODE_COMMAND);
	}
	else
		set_vi_mode(VI_MODE_VISUAL);
}

static void cmd_mode_visual_line(CmdContext *c, CmdParams *p)
{
	if (get_vi_mode() == VI_MODE_VISUAL_LINE)
	{
		SSM(p->sci, SCI_SETEMPTYSELECTION, p->pos, 0);
		set_vi_mode(VI_MODE_COMMAND);
	}
	else
	{
		set_vi_mode(VI_MODE_VISUAL_LINE);
		/* just to force the scintilla notification callback to be called so we can
		 * select the current line */
		SSM(p->sci, SCI_LINEEND, 0, 0);
	}
}

static void cmd_mode_insert_after(CmdContext *c, CmdParams *p)
{
	cmd_mode_insert(c, p);
	if (p->pos < p->line_end_pos)
		SSM(p->sci, SCI_CHARRIGHT, 0, 0);
}

static void goto_nonempty(CmdParams *p, gint line, gboolean scroll)
{
	gint line_end_pos = SSM(p->sci, SCI_GETLINEENDPOSITION, line, 0);
	gint pos = SSM(p->sci, SCI_POSITIONFROMLINE, line, 0);

	while (g_ascii_isspace(SSM(p->sci, SCI_GETCHARAT, pos, 0)) && pos < line_end_pos)
		pos = NEXT(p->sci, pos);
	SET_POS(p->sci, pos, scroll);
}

static void cmd_mode_insert_line_start_nonempty(CmdContext *c, CmdParams *p)
{
	goto_nonempty(p, p->line, FALSE);
	cmd_mode_insert(c, p);
}

static void cmd_mode_insert_line_start(CmdContext *c, CmdParams *p)
{
	SET_POS(p->sci, p->line_start_pos, FALSE);
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
	// undo inserted indentation
	SSM(p->sci, SCI_DELLINELEFT, 0, 0);
	c->num = p->num;
	c->newline_insert = TRUE;
	set_vi_mode(VI_MODE_INSERT);
}

static void cmd_mode_insert_prev_line(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_HOME, 0, 0);
	SSM(p->sci, SCI_NEWLINE, 0, 0);
	SSM(p->sci, SCI_LINEUP, 0, 0);
	c->num = p->num;
	c->newline_insert = TRUE;
	set_vi_mode(VI_MODE_INSERT);
}

static gint get_line_number_rel(CmdParams *p, gint shift)
{
	gint new_line = p->line + shift;
	new_line = new_line < 0 ? 0 : new_line;
	new_line = new_line > p->line_num ? p->line_num : new_line;
	return new_line;
}

static void cmd_mode_insert_cut_line(CmdContext *c, CmdParams *p)
{
	gint new_line = get_line_number_rel(p, p->num - 1);
	gint pos_end = SSM(p->sci, SCI_GETLINEENDPOSITION, new_line, 0);
	c->line_copy = TRUE;
	SSM(p->sci, SCI_COPYRANGE, p->line_start_pos, pos_end);
	SSM(p->sci, SCI_DELETERANGE, p->line_start_pos, pos_end - p->line_start_pos);
	cmd_mode_insert(c, p);
}

static void cmd_clear_right(CmdContext *c, CmdParams *p)
{
	gint new_line = get_line_number_rel(p, p->num - 1);
	gint pos = SSM(p->sci, SCI_GETLINEENDPOSITION, new_line, 0);
	c->line_copy = FALSE;
	SSM(p->sci, SCI_COPYRANGE, p->pos, pos);
	SSM(p->sci, SCI_DELETERANGE, p->pos, pos - p->pos);
}

static void cmd_mode_insert_clear_right(CmdContext *c, CmdParams *p)
{
	cmd_clear_right(c, p);
	cmd_mode_insert(c, p);
}

static void cmd_mode_insert_delete_char_yank(CmdContext *c, CmdParams *p)
{
	gint end = REL(p->sci, p->pos, p->num);
	end = end > p->line_end_pos ? p->line_end_pos : end;
	c->line_copy = FALSE;
	SSM(p->sci, SCI_COPYRANGE, p->pos, end);
	SSM(p->sci, SCI_DELETERANGE, p->pos, end - p->pos);
	cmd_mode_insert(c, p);
}

static void cmd_goto_page_up(CmdContext *c, CmdParams *p)
{
	gint shift = p->line_visible_num * p->num;
	gint new_line = get_line_number_rel(p, -shift);
	goto_nonempty(p, new_line, TRUE);
}

static void cmd_goto_page_down(CmdContext *c, CmdParams *p)
{
	gint shift = p->line_visible_num * p->num;
	gint new_line = get_line_number_rel(p, shift);
	goto_nonempty(p, new_line, TRUE);
}

static void cmd_goto_halfpage_up(CmdContext *c, CmdParams *p)
{
	gint shift = p->num_present ? p->num : p->line_visible_num / 2;
	gint new_line = get_line_number_rel(p, -shift);
	goto_nonempty(p, new_line, TRUE);
}

static void cmd_goto_halfpage_down(CmdContext *c, CmdParams *p)
{
	gint shift = p->num_present ? p->num : p->line_visible_num / 2;
	gint new_line = get_line_number_rel(p, shift);
	goto_nonempty(p, new_line, TRUE);
}

static void cmd_scroll_up(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_LINESCROLL, 0, -p->num);
}

static void cmd_scroll_down(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_LINESCROLL, 0, p->num);
}

static void scroll_to_line(CmdParams *p, gint offset, gboolean nonempty)
{
	gint column = SSM(p->sci, SCI_GETCOLUMN, p->pos, 0);
	gint line = p->line;

	if (p->num_present)
		line = p->num - 1;
	if (nonempty)
		goto_nonempty(p, line, FALSE);
	else
	{
		gint pos = SSM(p->sci, SCI_FINDCOLUMN, line, column);
		SET_POS(p->sci, pos, FALSE);
	}
	SSM(p->sci, SCI_SETFIRSTVISIBLELINE, line + offset, 0);
}

static void cmd_scroll_center(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, - p->line_visible_num / 2, FALSE);
}

static void cmd_scroll_top(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, 0, FALSE);
}

static void cmd_scroll_bottom(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, - p->line_visible_num + 1, FALSE);
}

static void cmd_scroll_center_nonempty(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, - p->line_visible_num / 2, TRUE);
}

static void cmd_scroll_top_nonempty(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, 0, TRUE);
}

static void cmd_scroll_top_next_nonempty(CmdContext *c, CmdParams *p)
{
	if (p->num_present)
		cmd_scroll_top_nonempty(c, p);
	else
	{
		gint line = p->line_visible_first + p->line_visible_num;
		goto_nonempty(p, line, FALSE);
		SSM(p->sci, SCI_SETFIRSTVISIBLELINE, line, 0);
	}
}

static void cmd_scroll_bottom_nonempty(CmdContext *c, CmdParams *p)
{
	scroll_to_line(p, - p->line_visible_num + 1, TRUE);
}

static void cmd_goto_left(CmdContext *c, CmdParams *p)
{
	gint i;
	gint start_pos = p->line_start_pos;
	gint pos = p->pos;
	for (i = 0; i < p->num && pos > start_pos; i++)
	{
		pos = PREV(p->sci, pos);
		SET_POS(p->sci, pos, FALSE);
	}
}

static void cmd_goto_right(CmdContext *c, CmdParams *p)
{
	gint i;
	gint pos = p->pos;
	for (i = 0; i < p->num && pos < p->line_end_pos; i++)
	{
		pos = NEXT(p->sci, pos);
		SET_POS(p->sci, pos, FALSE);
	}
}

static void cmd_goto_up(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_LINEUP, 0, 0);
}

static void cmd_goto_up_nonempty(CmdContext *c, CmdParams *p)
{
	cmd_goto_up(c, p);
	goto_nonempty(p, GET_CUR_LINE(p->sci), TRUE);
}

static void cmd_goto_down(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_LINEDOWN, 0, 0);
}

static void cmd_goto_down_nonempty(CmdContext *c, CmdParams *p)
{
	cmd_goto_down(c, p);
	goto_nonempty(p, GET_CUR_LINE(p->sci), TRUE);
}

static void cmd_goto_down_one_less_nonempty(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num - 1; i++)
		SSM(p->sci, SCI_LINEDOWN, 0, 0);
	goto_nonempty(p, GET_CUR_LINE(p->sci), TRUE);
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
	gint num = get_line_number_rel(p, p->num);
	gint end = SSM(p->sci, SCI_POSITIONFROMLINE, num, 0);
	c->line_copy = TRUE;
	SSM(p->sci, SCI_COPYRANGE, p->line_start_pos, end);
}

static void cmd_paste(CmdContext *c, CmdParams *p, gboolean after)
{
	gint pos;
	gint i;

	if (c->line_copy)
	{
		if (after)
			pos = SSM(p->sci, SCI_POSITIONFROMLINE, p->line+1, 0);
		else
			pos = p->line_start_pos;
	}
	else
	{
		pos = p->pos;
		if (after && pos < p->line_end_pos)
			pos = NEXT(p->sci, pos);
	}

	SET_POS(p->sci, pos, TRUE);
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_PASTE, 0, 0);
	if (c->line_copy)
		SET_POS(p->sci, pos, TRUE);
	else if (!IS_INSERT(get_vi_mode()))
		SSM(p->sci, SCI_CHARLEFT, 0, 0);
}

static void cmd_paste_after(CmdContext *c, CmdParams *p)
{
	cmd_paste(c, p, TRUE);
}

static void cmd_paste_before(CmdContext *c, CmdParams *p)
{
	cmd_paste(c, p, FALSE);
}

static void cmd_delete_line(CmdContext *c, CmdParams *p)
{
	gint num = get_line_number_rel(p, p->num);
	gint end = SSM(p->sci, SCI_POSITIONFROMLINE, num, 0);
	c->line_copy = TRUE;
	SSM(p->sci, SCI_COPYRANGE, p->line_start_pos, end);
	SSM(p->sci, SCI_DELETERANGE, p->line_start_pos, end - p->line_start_pos);
}

static void cmd_search_next(CmdContext *c, CmdParams *p)
{
	gboolean invert = FALSE;

	if (p->last_kp->key == GDK_KEY_N)
		invert = TRUE;

	perform_search(p->sci, c, p->num, invert);
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
	perform_search(p->sci, c, p->num, FALSE);
}

static void cmd_search_current_next(CmdContext *c, CmdParams *p)
{
	search_current(c, p, TRUE);
}

static void cmd_search_current_prev(CmdContext *c, CmdParams *p)
{
	search_current(c, p, FALSE);
}

static void delete_char(CmdContext *c, CmdParams *p, gboolean yank)
{
	gint end_pos = REL(p->sci, p->pos, p->num);
	end_pos = end_pos > p->line_end_pos ? p->line_end_pos : end_pos;
	if (yank)
	{
		c->line_copy = FALSE;
		SSM(p->sci, SCI_COPYRANGE, p->pos, end_pos);
	}
	SSM(p->sci, SCI_DELETERANGE, p->pos, end_pos - p->pos);
}

static void cmd_delete_char(CmdContext *c, CmdParams *p)
{
	delete_char(c, p, FALSE);
}

static void cmd_delete_char_yank(CmdContext *c, CmdParams *p)
{
	delete_char(c, p, TRUE);
}

static void delete_char_back(CmdContext *c, CmdParams *p, gboolean yank)
{
	gint start_pos = REL(p->sci, p->pos, -p->num);
	start_pos = start_pos < p->line_start_pos ? p->line_start_pos : start_pos;
	if (yank)
	{
		c->line_copy = FALSE;
		SSM(p->sci, SCI_COPYRANGE, start_pos, p->pos);
	}
	SSM(p->sci, SCI_DELETERANGE, start_pos, p->pos - start_pos);
}

static void cmd_delete_char_back(CmdContext *c, CmdParams *p)
{
	delete_char_back(c, p, FALSE);
}

static void cmd_delete_char_back_yank(CmdContext *c, CmdParams *p)
{
	delete_char_back(c, p, TRUE);
}

static void cmd_goto_line(CmdContext *c, CmdParams *p)
{
	gint num = p->num > p->line_num ? p->line_num : p->num;
	goto_nonempty(p, num - 1, TRUE);
}

static void cmd_goto_line_last(CmdContext *c, CmdParams *p)
{
	gint num = p->num > p->line_num ? p->line_num : p->num;
	if (!p->num_present)
		num = p->line_num;
	goto_nonempty(p, num - 1, TRUE);
}

static void cmd_join_lines(CmdContext *c, CmdParams *p)
{
	//TODO: remove whitespace between lines
	gint next_line = get_line_number_rel(p, p->num);
	gint next_line_pos = SSM(p->sci, SCI_POSITIONFROMLINE, next_line, 0);

	SSM(p->sci, SCI_SETTARGETSTART, p->pos, 0);
	SSM(p->sci, SCI_SETTARGETEND, next_line_pos, 0);
	SSM(p->sci, SCI_LINESJOIN, 0, 0);
}

static void cmd_join_lines_vis(CmdContext *c, CmdParams *p)
{
	//TODO: remove whitespace between lines
	SSM(p->sci, SCI_SETTARGETSTART, p->sel_start, 0);
	SSM(p->sci, SCI_SETTARGETEND, p->sel_start + p->sel_len, 0);
	SSM(p->sci, SCI_LINESJOIN, 0, 0);
}

static void cmd_goto_next_word(CmdContext *c, CmdParams *p)
{
	//TODO: words in scintilla are different from words in vim
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_WORDRIGHT, 0, 0);
}

static void cmd_goto_next_word_end(CmdContext *c, CmdParams *p)
{
	//TODO: words in scintilla are different from words in vim
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_WORDRIGHTEND, 0, 0);
}

static void cmd_goto_previous_word(CmdContext *c, CmdParams *p)
{
	//TODO: words in scintilla are different from words in vim
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_WORDLEFT, 0, 0);
}

static void cmd_goto_previous_word_end(CmdContext *c, CmdParams *p)
{
	//TODO: words in scintilla are different from words in vim
	gint i;
	for (i = 0; i < p->num; i++)
		SSM(p->sci, SCI_WORDLEFTEND, 0, 0);
	SSM(p->sci, SCI_CHARLEFT, 0, 0);
}

static void cmd_goto_line_start(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_HOME, 0, 0);
}

static void cmd_goto_line_start_nonempty(CmdContext *c, CmdParams *p)
{
	goto_nonempty(p, p->line, FALSE);
}

static void cmd_goto_line_end(CmdContext *c, CmdParams *p)
{
	gint i;
	for (i = 0; i < p->num; i++)
	{
		SSM(p->sci, SCI_LINEEND, 0, 0);
		if (i != p->num - 1)
		{
			gint pos = NEXT(p->sci, SSM(p->sci, SCI_GETCURRENTPOS, 0, 0));
			SET_POS(p->sci, pos, TRUE);
		}
	}
}

static void cmd_goto_column(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_FINDCOLUMN, p->line, p->num - 1);
	SET_POS(p->sci, pos, TRUE);
}

static void cmd_goto_matching_brace(CmdContext *c, CmdParams *p)
{
	gint pos = SSM(p->sci, SCI_BRACEMATCH, p->pos, 0);
	if (pos != -1)
		SET_POS(p->sci, pos, TRUE);
}

static void cmd_goto_doc_percentage(CmdContext *c, CmdParams *p)
{
	if (p->num > 100)
		p->num = 100;

	goto_nonempty(p, (p->line_num * p->num) / 100, TRUE);
}

static void cmd_goto_screen_top(CmdContext *c, CmdParams *p)
{
	gint top = p->line_visible_first;
	gint count = p->line_visible_num;
	gint line = top + p->num;
	goto_nonempty(p, line > top + count ? top + count : line, FALSE);
}

static void cmd_goto_screen_middle(CmdContext *c, CmdParams *p)
{
	goto_nonempty(p, p->line_visible_first + p->line_visible_num/2, FALSE);
}

static void cmd_goto_screen_bottom(CmdContext *c, CmdParams *p)
{
	gint top = p->line_visible_first;
	gint count = p->line_visible_num;
	gint line = top + count - p->num;
	goto_nonempty(p, line < top ? top : line, FALSE);
}

static void replace_char(CmdParams *p, gint pos, gint num, gint line)
{
	const gchar *repl = kp_to_str(p->last_kp);
	gint i;

	for (i = 0; i < num; i++)
	{
		gint next_pos = NEXT(p->sci, pos);
		gint curr_line = SSM(p->sci, SCI_LINEFROMPOSITION, pos, 0);
		gint line_end_pos = SSM(p->sci, SCI_GETLINEENDPOSITION, curr_line, 0);

		if (line != -1 && pos == line_end_pos)
			break;

		if (pos != line_end_pos) //skip newline replacement
		{
			SSM(p->sci, SCI_SETTARGETSTART, pos, 0);
			SSM(p->sci, SCI_SETTARGETEND, next_pos, 0);
			SSM(p->sci, SCI_REPLACETARGET, -1, (sptr_t)repl);
		}

		// the next position has changed because of the replacement - call NEXT() again
		pos = NEXT(p->sci, pos);
	}
	SET_POS(p->sci, PREV(p->sci, pos), FALSE);
}

static void cmd_replace_char(CmdContext *c, CmdParams *p)
{
	replace_char(p, p->pos, p->num, p->line);
}

static void cmd_replace_char_vis(CmdContext *c, CmdParams *p)
{
	gint num = SSM(p->sci, SCI_COUNTCHARACTERS, p->sel_start, p->sel_start + p->sel_len);
	replace_char(p, p->sel_start, num, -1);
	SET_POS(p->sci, p->sel_start, FALSE);
	set_vi_mode(VI_MODE_COMMAND);
}

static void change_case(ScintillaObject *sci, gint pos, gint num, gint line,
	gboolean force_upper, gboolean force_lower)
{
	gint i;
	for (i = 0; i < num; i++)
	{
		gint line_end_pos = SSM(sci, SCI_GETLINEENDPOSITION, line, 0);
		gint next_pos = NEXT(sci, pos);
		struct Sci_TextRange tr;
		gchar *uppercase, *lowercase, *replacement;
		gchar buf[MAX_CHAR_SIZE];
		gunichar c;

		if (pos == next_pos || (line != -1 && next_pos > line_end_pos))
			break;

		tr.chrg.cpMin = pos;
		tr.chrg.cpMax = next_pos;
		tr.lpstrText = buf;
		SSM(sci, SCI_GETTEXTRANGE, 0, (sptr_t)&tr);

		c = g_utf8_get_char(buf);
		uppercase = g_utf8_strup(buf, -1);
		lowercase = g_utf8_strdown(buf, -1);
		replacement = g_unichar_islower(c) ? uppercase : lowercase;
		replacement = force_upper ? uppercase : replacement;
		replacement = force_lower ? lowercase : replacement;

		SSM(sci, SCI_SETTARGETSTART, pos, 0);
		SSM(sci, SCI_SETTARGETEND, next_pos, 0);
		SSM(sci, SCI_REPLACETARGET, -1, (sptr_t)replacement);

		// the next position has changed because of the replacement - call NEXT() again
		pos = NEXT(sci, pos);
		SET_POS(sci, pos, FALSE);
		g_free(uppercase);
		g_free(lowercase);
	}
}

static void switch_case(CmdContext *c, CmdParams *p,
	gboolean force_upper, gboolean force_lower)
{
	if (IS_VISUAL(get_vi_mode()) || p->sel_len > 0)
	{
		gint num = SSM(p->sci, SCI_COUNTCHARACTERS, p->sel_start, p->sel_start + p->sel_len);
		change_case(p->sci, p->sel_start, num, -1, force_upper, force_lower);
		set_vi_mode(VI_MODE_COMMAND);
	}
	else
		change_case(p->sci, p->pos, p->num, p->line, force_upper, force_lower);
}

static void cmd_switch_case(CmdContext *c, CmdParams *p)
{
	switch_case(c, p, FALSE, FALSE);
}

static void cmd_lower_case(CmdContext *c, CmdParams *p)
{
	switch_case(c, p, FALSE, TRUE);
}

static void cmd_upper_case(CmdContext *c, CmdParams *p)
{
	switch_case(c, p, TRUE, FALSE);
}

static void cmd_find_char(CmdContext *c, CmdParams *p, gboolean invert)
{
	struct Sci_TextToFind ttf;
	gboolean forward;
	gint pos = p->pos;
	gint i;

	if (!c->search_char)
		return;

	forward = c->search_char[0] == 'f' || c->search_char[0] == 't';
	forward = !forward != !invert;
	ttf.lpstrText = c->search_char + 1;

	for (i = 0; i < p->num; i++)
	{
		gint new_pos;

		if (forward)
		{
			ttf.chrg.cpMin = NEXT(p->sci, pos);
			ttf.chrg.cpMax = p->line_end_pos;
		}
		else
		{
			ttf.chrg.cpMin = pos;
			ttf.chrg.cpMax = p->line_start_pos;
		}

		new_pos = SSM(p->sci, SCI_FINDTEXT, 0, (sptr_t)&ttf);
		if (new_pos < 0)
			break;
		pos = new_pos;
	}

	if (pos >= 0)
	{
		if (c->search_char[0] == 't')
			pos = PREV(p->sci, pos);
		else if (c->search_char[0] == 'T')
			pos = NEXT(p->sci, pos);
		SET_POS(p->sci, pos, FALSE);
	}
}

static void cmd_find_next_char(CmdContext *c, CmdParams *p)
{
	g_free(c->search_char);
	c->search_char = g_strconcat("f", kp_to_str(p->last_kp), NULL);
	cmd_find_char(c, p, FALSE);
}

static void cmd_find_prev_char(CmdContext *c, CmdParams *p)
{
	g_free(c->search_char);
	c->search_char = g_strconcat("F", kp_to_str(p->last_kp), NULL);
	cmd_find_char(c, p, FALSE);
}

static void cmd_find_next_char_before(CmdContext *c, CmdParams *p)
{
	g_free(c->search_char);
	c->search_char = g_strconcat("t", kp_to_str(p->last_kp), NULL);
	cmd_find_char(c, p, FALSE);
}

static void cmd_find_prev_char_before(CmdContext *c, CmdParams *p)
{
	g_free(c->search_char);
	c->search_char = g_strconcat("T", kp_to_str(p->last_kp), NULL);
	cmd_find_char(c, p, FALSE);
}

static void cmd_find_char_repeat(CmdContext *c, CmdParams *p)
{
	cmd_find_char(c, p, FALSE);
}

static void cmd_find_char_repeat_opposite(CmdContext *c, CmdParams *p)
{
	cmd_find_char(c, p, TRUE);
}

static void indent(ScintillaObject *sci, gboolean unindent, gint pos, gint num, gint line_num)
{
	gint i;

	SET_POS(sci, pos, FALSE);
	for (i = 0; i < num; i++)
	{
		gint line;
		SSM(sci, SCI_HOME, 0, 0);
		if (unindent)
			SSM(sci, SCI_BACKTAB, 0, 0);
		else
			SSM(sci, SCI_TAB, 0, 0);
		line = GET_CUR_LINE(sci);
		if (line == line_num - 1)
			break;
		SSM(sci, SCI_LINEDOWN, 0, 0);
	}
	SET_POS(sci, pos, FALSE);
}

static void cmd_indent(CmdContext *c, CmdParams *p)
{
	indent(p->sci, FALSE, p->pos, p->num, p->line_num);
}

static void cmd_unindent(CmdContext *c, CmdParams *p)
{
	indent(p->sci, TRUE, p->pos, p->num, p->line_num);
}

static void range_indent(CmdContext *c, CmdParams *p, gboolean unindent)
{
	indent(p->sci, unindent, p->sel_start, p->sel_last_line - p->sel_first_line + 1, p->line_num);
	set_vi_mode(VI_MODE_COMMAND);
}

static void cmd_range_indent(CmdContext *c, CmdParams *p)
{
	range_indent(c, p, FALSE);
}

static void cmd_range_unindent(CmdContext *c, CmdParams *p)
{
	range_indent(c, p, TRUE);
}

static void cmd_range_delete(CmdContext *c, CmdParams *p)
{
	gint sel_end_pos = p->sel_start + p->sel_len;
	if (p->is_operator_cmd && p->line_end_pos < sel_end_pos)
		sel_end_pos = p->line_end_pos;
	c->line_copy = FALSE;
	SSM(p->sci, SCI_COPYRANGE, p->sel_start, sel_end_pos);

	SSM(p->sci, SCI_DELETERANGE, p->sel_start, sel_end_pos - p->sel_start);
	set_vi_mode(VI_MODE_COMMAND);
}

static void cmd_range_copy(CmdContext *c, CmdParams *p)
{
	gint sel_end_pos = p->sel_start + p->sel_len;
	if (p->is_operator_cmd && p->line_end_pos < sel_end_pos)
		sel_end_pos = p->line_end_pos;
	c->line_copy = FALSE;
	SSM(p->sci, SCI_COPYRANGE, p->sel_start, sel_end_pos);
	
	SSM(p->sci, SCI_SETCURRENTPOS, p->sel_start, 0);
	set_vi_mode(VI_MODE_COMMAND);
}

static void cmd_range_change(CmdContext *c, CmdParams *p)
{
	gint sel_end_pos = p->sel_start + p->sel_len;
	if (p->is_operator_cmd && p->line_end_pos < sel_end_pos)
		sel_end_pos = p->line_end_pos;
	c->line_copy = FALSE;
	SSM(p->sci, SCI_COPYRANGE, p->sel_start, sel_end_pos);

	SSM(p->sci, SCI_DELETERANGE, p->sel_start, sel_end_pos - p->sel_start);
	cmd_mode_insert(c, p);
}

static void cmd_delete_lines_insert_vis(CmdContext *c, CmdParams *p)
{
	gint begin = p->sel_first_line_begin_pos;
	gint end = p->sel_last_line_end_pos;
	c->line_copy = TRUE;
	SSM(p->sci, SCI_COPYRANGE, begin, end);

	SSM(p->sci, SCI_DELETERANGE, begin, end - begin);
	cmd_mode_insert(c, p);
}

static void cmd_delete_lines_vis(CmdContext *c, CmdParams *p)
{
	gint begin = p->sel_first_line_begin_pos;
	gint end = p->sel_last_line_end_pos;
	c->line_copy = TRUE;
	SSM(p->sci, SCI_COPYRANGE, begin, end);

	SSM(p->sci, SCI_DELETERANGE, begin, end - begin);
	SET_POS(p->sci, begin, TRUE);
	set_vi_mode(VI_MODE_COMMAND);
}

static void cmd_yank_lines_vis(CmdContext *c, CmdParams *p)
{
	gint begin = p->sel_first_line_begin_pos;
	gint end = p->sel_last_line_end_pos;
	c->line_copy = TRUE;
	SSM(p->sci, SCI_COPYRANGE, begin, end);

	SET_POS(p->sci, begin, TRUE);
	set_vi_mode(VI_MODE_COMMAND);
}

static void cmd_repeat_last_command(CmdContext *c, CmdParams *p)
{
	// fake - handled in a special way
}

static void cmd_swap_anchor(CmdContext *c, CmdParams *p)
{
	gint anchor = c->sel_anchor;
	c->sel_anchor = p->pos;
	SET_POS(p->sci, anchor, FALSE);
}

static void cmd_escape(CmdContext *c, CmdParams *p)
{
	if (SSM(p->sci, SCI_AUTOCACTIVE, 0, 0) || SSM(p->sci, SCI_CALLTIPACTIVE, 0, 0))
		SSM(p->sci, SCI_CANCEL, 0, 0);
	else
		set_vi_mode(VI_MODE_COMMAND);
}

static void cmd_run_single_command(CmdContext *c, CmdParams *p)
{
	set_vi_mode(VI_MODE_COMMAND_SINGLE);
}

static void cmd_newline(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_NEWLINE, 0, 0);
}

static void cmd_tab(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_TAB, 0, 0);
}

static void cmd_del_word_left(CmdContext *c, CmdParams *p)
{
	SSM(p->sci, SCI_DELWORDLEFT, 0, 0);
}


static void indent_ins(CmdContext *c, CmdParams *p, gboolean indent)
{
	gint delta;
	SSM(p->sci, SCI_HOME, 0, 0);
	SSM(p->sci, indent ? SCI_TAB : SCI_BACKTAB, 0, 0);
	delta = SSM(p->sci, SCI_GETLINEENDPOSITION, p->line, 0) - p->line_end_pos;
	SET_POS(p->sci, p->pos + delta, FALSE);
}

static void cmd_indent_ins(CmdContext *c, CmdParams *p)
{
	indent_ins(c, p, TRUE);
}

static void cmd_unindent_ins(CmdContext *c, CmdParams *p)
{
	indent_ins(c, p, FALSE);
}

static void cmd_copy_char(CmdContext *c, CmdParams *p, gboolean above)
{
	if ((above && p->line > 0) || (!above && p->line < p->line_num - 1))
	{
		gint line = above ? p->line - 1 : p->line + 1;
		gint col = SSM(p->sci, SCI_GETCOLUMN, p->pos, 0);
		gint pos = SSM(p->sci, SCI_FINDCOLUMN, line, col);
		gint line_end = SSM(p->sci, SCI_GETLINEENDPOSITION, line, 0);

		if (pos < line_end)
		{
			gchar txt[MAX_CHAR_SIZE];
			struct Sci_TextRange tr;

			tr.chrg.cpMin = pos;
			tr.chrg.cpMax = NEXT(p->sci, pos);
			tr.lpstrText = txt;

			SSM(p->sci, SCI_GETTEXTRANGE, 0, (sptr_t)&tr);
			SSM(p->sci, SCI_INSERTTEXT, p->pos, (sptr_t)txt);
			SET_POS(p->sci, NEXT(p->sci, p->pos), FALSE);
		}
	}
}

static void cmd_copy_char_from_above(CmdContext *c, CmdParams *p)
{
	cmd_copy_char(c, p, TRUE);
}

static void cmd_copy_char_from_below(CmdContext *c, CmdParams *p)
{
	cmd_copy_char(c, p, FALSE);
}

static void cmd_paste_inserted_text(CmdContext *c, CmdParams *p)
{
	const gchar *txt = get_inserted_text();
	SSM(p->sci, SCI_ADDTEXT, strlen(txt), (sptr_t) txt);
}

static void cmd_paste_inserted_text_leave(CmdContext *c, CmdParams *p)
{
	cmd_paste_inserted_text(c, p);
	set_vi_mode(VI_MODE_COMMAND);
}

static gint find_upper_level_brace(ScintillaObject *sci, gint pos, gint open_brace, gint close_brace)
{
	while (pos > 0)
	{
		pos = PREV(sci, pos);
		gchar c = SSM(sci, SCI_GETCHARAT, pos, 0);

		if (c == open_brace)
			return pos;
		else if (c == close_brace) 
		{
			pos = SSM(sci, SCI_BRACEMATCH, pos, 0);
			if (pos < 0)
				break;
		}
	}
	return -1;
}

static gint find_char(ScintillaObject *sci, gint pos, gint ch, gboolean forward)
{
	while (pos > 0)
	{
		gint last_pos = pos;
		gchar c;

		pos = forward ? NEXT(sci, pos) : PREV(sci, pos);
		c = SSM(sci, SCI_GETCHARAT, pos, 0);

		if (c == ch)
			return pos;
		if (pos == last_pos)
			break;
	}
	return -1;
}

static void select_brace(CmdContext *c, CmdParams *p, gint open_brace, gint close_brace, gboolean inner)
{
	gint pos = p->pos;
	gint start_pos = 0;
	gint end_pos = 0;
	gint i;

	for (i = 0; i < p->num; i++)
	{
		if (open_brace == close_brace)
		{
			start_pos = find_char(p->sci, pos, open_brace, FALSE);
			if (start_pos < 0)
				return;
			end_pos = find_char(p->sci, pos, close_brace, TRUE);
		}
		else
		{
			start_pos = find_upper_level_brace(p->sci, pos, open_brace, close_brace);
			if (start_pos < 0)
				return;
			end_pos = SSM(p->sci, SCI_BRACEMATCH, start_pos, 0);
		}

		if (end_pos < 0)
			return;

		pos = start_pos;
	}

	if (inner)
		start_pos = NEXT(p->sci, start_pos);
	else
		end_pos = NEXT(p->sci, end_pos);
	
	if (IS_VISUAL(get_vi_mode()))
	{
		c->sel_anchor = start_pos;
		SET_POS(p->sci, end_pos, TRUE);
	}
	else
	{
		p->sel_start = start_pos;
		p->sel_len = end_pos - start_pos;
	}
}

static void cmd_select_quotedbl(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '\"', '\"', FALSE);
}

static void cmd_select_quoteleft(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '`', '`', FALSE);
}

static void cmd_select_apostrophe(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '\'', '\'', FALSE);
}

static void cmd_select_brace(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '{', '}', FALSE);
}

static void cmd_select_paren(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '(', ')', FALSE);
}

static void cmd_select_less(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '<', '>', FALSE);
}

static void cmd_select_bracket(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '[', ']', FALSE);
}

static void cmd_select_quotedbl_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '\"', '\"', TRUE);
}

static void cmd_select_quoteleft_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '`', '`', TRUE);
}

static void cmd_select_apostrophe_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '\'', '\'', TRUE);
}

static void cmd_select_brace_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '{', '}', TRUE);
}

static void cmd_select_paren_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '(', ')', TRUE);
}

static void cmd_select_less_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '<', '>', TRUE);
}

static void cmd_select_bracket_inner(CmdContext *c, CmdParams *p)
{
	select_brace(c, p, '[', ']', TRUE);
}

static void cmd_nop(CmdContext *c, CmdParams *p)
{
	// do nothing
}

/******************************************************************************/


typedef struct {
	Cmd cmd;
	guint key1;
	guint key2;
	guint modif1;
	guint modif2;
	gboolean param;
	gboolean needs_selection;
} CmdDef;


#define ARROW_MOTIONS \
	{cmd_goto_next_word, GDK_KEY_Right, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_Right, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_KP_Right, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_KP_Right, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_rightarrow, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_rightarrow, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_Left, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_Left, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_KP_Left, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_KP_Left, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_leftarrow, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_leftarrow, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_up, GDK_KEY_Up, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_up, GDK_KEY_KP_Up, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_up, GDK_KEY_uparrow, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_down, GDK_KEY_Down, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_down, GDK_KEY_KP_Down, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_down, GDK_KEY_downarrow, 0, GDK_SHIFT_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_left, GDK_KEY_Left, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_left, GDK_KEY_KP_Left, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_left, GDK_KEY_leftarrow, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_right, GDK_KEY_Right, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_right, GDK_KEY_KP_Right, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_right, GDK_KEY_rightarrow, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_up, GDK_KEY_Up, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_up, GDK_KEY_KP_Up, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_up, GDK_KEY_uparrow, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down, GDK_KEY_Down, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down, GDK_KEY_KP_Down, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down, GDK_KEY_downarrow, 0, 0, 0, FALSE, FALSE}, \
	/* END */


#define MOVEMENT_CMDS \
	ARROW_MOTIONS \
	/* left */ \
	{cmd_goto_left, GDK_KEY_h, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_left, GDK_KEY_h, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_left, GDK_KEY_BackSpace, 0, 0, 0, FALSE, FALSE}, \
	/* right */ \
	{cmd_goto_right, GDK_KEY_l, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_right, GDK_KEY_space, 0, 0, 0, FALSE, FALSE}, \
	/* line beginning */ \
	{cmd_goto_line_start, GDK_KEY_0, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_line_start, GDK_KEY_Home, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_line_start_nonempty, GDK_KEY_asciicircum, 0, 0, 0, FALSE, FALSE}, \
	/* line end */ \
	{cmd_goto_line_end, GDK_KEY_dollar, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_line_end, GDK_KEY_End, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_column, GDK_KEY_bar, 0, 0, 0, FALSE, FALSE}, \
	/* find character */ \
	{cmd_find_next_char, GDK_KEY_f, 0, 0, 0, TRUE, FALSE}, \
	{cmd_find_prev_char, GDK_KEY_F, 0, 0, 0, TRUE, FALSE}, \
	{cmd_find_next_char_before, GDK_KEY_t, 0, 0, 0, TRUE, FALSE}, \
	{cmd_find_prev_char_before, GDK_KEY_T, 0, 0, 0, TRUE, FALSE}, \
	{cmd_find_char_repeat, GDK_KEY_semicolon, 0, 0, 0, FALSE, FALSE}, \
	{cmd_find_char_repeat_opposite, GDK_KEY_comma, 0, 0, 0, FALSE, FALSE}, \
	/* up */ \
	{cmd_goto_up, GDK_KEY_k, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_up, GDK_KEY_p, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_up_nonempty, GDK_KEY_minus, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_up_nonempty, GDK_KEY_KP_Subtract, 0, 0, 0, FALSE, FALSE}, \
	/* down */ \
	{cmd_goto_down, GDK_KEY_j, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down, GDK_KEY_j, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_down, GDK_KEY_n, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_plus, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_KP_Add, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_m, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_Return, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_KP_Enter, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down_nonempty, GDK_KEY_ISO_Enter, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_down_one_less_nonempty, GDK_KEY_underscore, 0, 0, 0, FALSE, FALSE}, \
	/* goto line */ \
	{cmd_goto_line_last, GDK_KEY_G, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_line_last, GDK_KEY_End, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_line_last, GDK_KEY_KP_End, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_line, GDK_KEY_g, GDK_KEY_g, 0, 0, FALSE, FALSE}, \
	{cmd_goto_line, GDK_KEY_Home, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_line, GDK_KEY_KP_Home, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_doc_percentage, GDK_KEY_percent, 0, 0, 0, FALSE, FALSE}, \
	/* goto next/prev word */ \
	{cmd_goto_next_word, GDK_KEY_w, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_next_word, GDK_KEY_W, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_next_word_end, GDK_KEY_e, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_next_word_end, GDK_KEY_E, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_b, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word, GDK_KEY_B, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word_end, GDK_KEY_g, GDK_KEY_e, 0, 0, FALSE, FALSE}, \
	{cmd_goto_previous_word_end, GDK_KEY_g, GDK_KEY_E, 0, 0, FALSE, FALSE}, \
	/* various motions */ \
	{cmd_goto_matching_brace, GDK_KEY_percent, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_screen_top, GDK_KEY_H, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_screen_middle, GDK_KEY_M, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_screen_bottom, GDK_KEY_L, 0, 0, 0, FALSE, FALSE}, \
	/* scrolling */ \
	{cmd_goto_page_down, GDK_KEY_f, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_down, GDK_KEY_Page_Down, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_page_down, GDK_KEY_KP_Page_Down, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_page_up, GDK_KEY_b, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_page_up, GDK_KEY_Page_Up, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_page_up, GDK_KEY_KP_Page_Up, 0, 0, 0, FALSE, FALSE}, \
	{cmd_goto_halfpage_down, GDK_KEY_d, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_goto_halfpage_up, GDK_KEY_u, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	/* END */


CmdDef movement_cmds[] = {
	MOVEMENT_CMDS
	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


#define OPERATOR_CMDS \
	{cmd_range_delete, GDK_KEY_d, 0, 0, 0, FALSE, TRUE}, \
	{cmd_range_copy, GDK_KEY_y, 0, 0, 0, FALSE, TRUE}, \
	{cmd_range_change, GDK_KEY_c, 0, 0, 0, FALSE, TRUE}, \
	{cmd_range_unindent, GDK_KEY_less, 0, 0, 0, FALSE, TRUE}, \
	{cmd_range_indent, GDK_KEY_greater, 0, 0, 0, FALSE, TRUE}, \
	{cmd_switch_case, GDK_KEY_g, GDK_KEY_asciitilde, 0, 0, FALSE, TRUE}, \
	{cmd_switch_case, GDK_KEY_asciitilde, 0, 0, 0, FALSE, TRUE}, \
	{cmd_upper_case, GDK_KEY_g, GDK_KEY_U, 0, 0, FALSE, TRUE}, \
	{cmd_lower_case, GDK_KEY_g, GDK_KEY_u, 0, 0, FALSE, TRUE}, \
	/* END */


CmdDef operator_cmds[] = {
	OPERATOR_CMDS
	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


#define TEXT_OBJECT_CMDS \
	{cmd_select_quotedbl, GDK_KEY_a, GDK_KEY_quotedbl, 0, 0, FALSE, FALSE}, \
	{cmd_select_quoteleft, GDK_KEY_a, GDK_KEY_quoteleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_apostrophe, GDK_KEY_a, GDK_KEY_apostrophe, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace, GDK_KEY_a, GDK_KEY_braceleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace, GDK_KEY_a, GDK_KEY_braceright, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace, GDK_KEY_a, GDK_KEY_B, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren, GDK_KEY_a, GDK_KEY_parenleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren, GDK_KEY_a, GDK_KEY_parenright, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren, GDK_KEY_a, GDK_KEY_b, 0, 0, FALSE, FALSE}, \
	{cmd_select_less, GDK_KEY_a, GDK_KEY_less, 0, 0, FALSE, FALSE}, \
	{cmd_select_less, GDK_KEY_a, GDK_KEY_greater, 0, 0, FALSE, FALSE}, \
	{cmd_select_bracket, GDK_KEY_a, GDK_KEY_bracketleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_bracket, GDK_KEY_a, GDK_KEY_bracketright, 0, 0, FALSE, FALSE}, \
	/* inner */ \
	{cmd_select_quotedbl_inner, GDK_KEY_i, GDK_KEY_quotedbl, 0, 0, FALSE, FALSE}, \
	{cmd_select_quoteleft_inner, GDK_KEY_i, GDK_KEY_quoteleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_apostrophe_inner, GDK_KEY_i, GDK_KEY_apostrophe, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace_inner, GDK_KEY_i, GDK_KEY_braceleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace_inner, GDK_KEY_i, GDK_KEY_braceright, 0, 0, FALSE, FALSE}, \
	{cmd_select_brace_inner, GDK_KEY_i, GDK_KEY_B, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren_inner, GDK_KEY_i, GDK_KEY_parenleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren_inner, GDK_KEY_i, GDK_KEY_parenright, 0, 0, FALSE, FALSE}, \
	{cmd_select_paren_inner, GDK_KEY_i, GDK_KEY_b, 0, 0, FALSE, FALSE}, \
	{cmd_select_less_inner, GDK_KEY_i, GDK_KEY_less, 0, 0, FALSE, FALSE}, \
	{cmd_select_less_inner, GDK_KEY_i, GDK_KEY_greater, 0, 0, FALSE, FALSE}, \
	{cmd_select_bracket_inner, GDK_KEY_i, GDK_KEY_bracketleft, 0, 0, FALSE, FALSE}, \
	{cmd_select_bracket_inner, GDK_KEY_i, GDK_KEY_bracketright, 0, 0, FALSE, FALSE}, \
	/* END */


CmdDef text_object_cmds[] = {
	TEXT_OBJECT_CMDS
	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


#define SCROLLING_CMDS \
	{cmd_scroll_down, GDK_KEY_e, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_scroll_up, GDK_KEY_y, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE}, \
	{cmd_scroll_center, GDK_KEY_z, GDK_KEY_z, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_top, GDK_KEY_z, GDK_KEY_t, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_bottom, GDK_KEY_z, GDK_KEY_b, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_center_nonempty, GDK_KEY_z, GDK_KEY_period, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_top_nonempty, GDK_KEY_z, GDK_KEY_Return, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_top_nonempty, GDK_KEY_z, GDK_KEY_KP_Enter, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_top_nonempty, GDK_KEY_z, GDK_KEY_ISO_Enter, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_top_next_nonempty, GDK_KEY_z, GDK_KEY_plus, 0, 0, FALSE, FALSE}, \
	{cmd_scroll_bottom_nonempty, GDK_KEY_z, GDK_KEY_minus, 0, 0, FALSE, FALSE}, \
	/* END */


#define ENTER_CMDLINE_CMDS \
	{cmd_mode_cmdline, GDK_KEY_colon, 0, 0, 0, FALSE, FALSE}, \
	{cmd_mode_cmdline, GDK_KEY_slash, 0, 0, 0, FALSE, FALSE}, \
	{cmd_mode_cmdline, GDK_KEY_KP_Divide, 0, 0, 0, FALSE, FALSE}, \
	{cmd_mode_cmdline, GDK_KEY_question, 0, 0, 0, FALSE, FALSE}, \
	/* END */


#define SEARCH_CMDS \
	{cmd_search_next, GDK_KEY_n, 0, 0, 0, FALSE, FALSE}, \
	{cmd_search_next, GDK_KEY_N, 0, 0, 0, FALSE, FALSE}, \
	{cmd_search_current_next, GDK_KEY_asterisk, 0, 0, 0, FALSE, FALSE}, \
	{cmd_search_current_next, GDK_KEY_KP_Multiply, 0, 0, 0, FALSE, FALSE}, \
	{cmd_search_current_prev, GDK_KEY_numbersign, 0, 0, 0, FALSE, FALSE}, \
	/* END */


CmdDef cmd_mode_cmds[] = {
	/* enter insert mode */
	{cmd_mode_insert_after, GDK_KEY_a, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_insert_line_end, GDK_KEY_A, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_insert, GDK_KEY_i, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_insert, GDK_KEY_Insert, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_insert, GDK_KEY_KP_Insert, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_insert_line_start_nonempty, GDK_KEY_I, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_insert_line_start, GDK_KEY_g, GDK_KEY_I, 0, 0, FALSE, FALSE},
	{cmd_mode_insert_next_line, GDK_KEY_o, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_insert_prev_line, GDK_KEY_O, 0, 0, 0, FALSE, FALSE},
	/* enter visual mode */
	{cmd_mode_visual, GDK_KEY_v, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_visual_line, GDK_KEY_V, 0, 0, 0, FALSE, FALSE},

	/* deletion */
	{cmd_delete_char_yank, GDK_KEY_x, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_char_yank, GDK_KEY_Delete, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_char_yank, GDK_KEY_KP_Delete, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_char_back_yank, GDK_KEY_X, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_line, GDK_KEY_d, GDK_KEY_d, 0, 0, FALSE, FALSE},
	{cmd_clear_right, GDK_KEY_D, 0, 0, 0, FALSE, FALSE},
	{cmd_join_lines, GDK_KEY_J, 0, 0, 0, FALSE, FALSE},

	/* copy/paste */
	{cmd_copy_line, GDK_KEY_y, GDK_KEY_y, 0, 0, FALSE, FALSE},
	{cmd_copy_line, GDK_KEY_Y, 0, 0, 0, FALSE, FALSE},
	{cmd_paste_after, GDK_KEY_p, 0, 0, 0, FALSE, FALSE},
	{cmd_paste_before, GDK_KEY_P, 0, 0, 0, FALSE, FALSE},

	/* changing text */
	{cmd_mode_replace, GDK_KEY_R, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_insert_cut_line, GDK_KEY_c, GDK_KEY_c, 0, 0, FALSE, FALSE},
	{cmd_mode_insert_cut_line, GDK_KEY_S, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_insert_clear_right, GDK_KEY_C, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_insert_delete_char_yank, GDK_KEY_s, 0, 0, 0, FALSE, FALSE},
	{cmd_replace_char, GDK_KEY_r, 0, 0, 0, TRUE, FALSE},
	{cmd_switch_case, GDK_KEY_asciitilde, 0, 0, 0, FALSE, FALSE},
	{cmd_unindent, GDK_KEY_less, GDK_KEY_less, 0, 0, FALSE, FALSE},
	{cmd_indent, GDK_KEY_greater, GDK_KEY_greater, 0, 0, FALSE, FALSE},

	/* undo/redo */
	{cmd_undo, GDK_KEY_U, 0, 0, 0, FALSE, FALSE},
	{cmd_undo, GDK_KEY_u, 0, 0, 0, FALSE, FALSE},
	{cmd_redo, GDK_KEY_r, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},

	/* special */
	{cmd_repeat_last_command, GDK_KEY_period, 0, 0, 0, FALSE, FALSE},
	{cmd_repeat_last_command, GDK_KEY_KP_Decimal, 0, 0, 0, FALSE, FALSE},
	{cmd_escape, GDK_KEY_Escape, 0, 0, 0, FALSE, FALSE},
	{cmd_nop, GDK_KEY_Insert, 0, 0, 0, FALSE, FALSE},
	{cmd_nop, GDK_KEY_KP_Insert, 0, 0, 0, FALSE, FALSE},

	SEARCH_CMDS
	MOVEMENT_CMDS
	TEXT_OBJECT_CMDS
	OPERATOR_CMDS
	SCROLLING_CMDS
	ENTER_CMDLINE_CMDS

	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


CmdDef vis_mode_cmds[] = {
	{cmd_escape, GDK_KEY_Escape, 0, 0, 0, FALSE, FALSE},
	{cmd_escape, GDK_KEY_c, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_swap_anchor, GDK_KEY_o, 0, 0, 0, FALSE, FALSE},
	{cmd_swap_anchor, GDK_KEY_O, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_visual, GDK_KEY_v, 0, 0, 0, FALSE, FALSE},
	{cmd_mode_visual_line, GDK_KEY_V, 0, 0, 0, FALSE, FALSE},
	{cmd_nop, GDK_KEY_Insert, 0, 0, 0, FALSE, FALSE},
	{cmd_nop, GDK_KEY_KP_Insert, 0, 0, 0, FALSE, FALSE},
	{cmd_upper_case, GDK_KEY_U, 0, 0, 0, FALSE, FALSE},
	{cmd_lower_case, GDK_KEY_u, 0, 0, 0, FALSE, FALSE},
	{cmd_join_lines_vis, GDK_KEY_J, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_lines_insert_vis, GDK_KEY_C, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_lines_insert_vis, GDK_KEY_S, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_lines_insert_vis, GDK_KEY_R, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_lines_vis, GDK_KEY_D, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_lines_vis, GDK_KEY_X, 0, 0, 0, FALSE, FALSE},
	{cmd_range_delete, GDK_KEY_x, 0, 0, 0, FALSE, FALSE},
	{cmd_range_delete, GDK_KEY_Delete, 0, 0, 0, FALSE, FALSE},
	{cmd_range_delete, GDK_KEY_KP_Delete, 0, 0, 0, FALSE, FALSE},
	{cmd_range_change, GDK_KEY_s, 0, 0, 0, FALSE, FALSE},
	{cmd_yank_lines_vis, GDK_KEY_Y, 0, 0, 0, FALSE, FALSE},
	{cmd_replace_char_vis, GDK_KEY_r, 0, 0, 0, TRUE, FALSE},

	SEARCH_CMDS
	MOVEMENT_CMDS
	TEXT_OBJECT_CMDS
	OPERATOR_CMDS
	SCROLLING_CMDS
	ENTER_CMDLINE_CMDS

	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};


CmdDef ins_mode_cmds[] = {
	{cmd_escape, GDK_KEY_Escape, 0, 0, 0, FALSE, FALSE},
	{cmd_escape, GDK_KEY_c, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_escape, GDK_KEY_bracketleft, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},

	{cmd_run_single_command, GDK_KEY_o, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},

	{cmd_goto_line_last, GDK_KEY_End, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_goto_line_last, GDK_KEY_KP_End, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_goto_line, GDK_KEY_Home, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_goto_line, GDK_KEY_KP_Home, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},

	{cmd_newline, GDK_KEY_m, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_newline, GDK_KEY_j, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_tab, GDK_KEY_i, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},

	{cmd_paste_inserted_text, GDK_KEY_a, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_paste_inserted_text_leave, GDK_KEY_at, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	/* it's enough to press Ctrl+2 instead of Ctrl+Shift+2 to get Ctrl+@ */
	{cmd_paste_inserted_text_leave, GDK_KEY_2, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},

	{cmd_delete_char, GDK_KEY_Delete, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_char, GDK_KEY_KP_Delete, 0, 0, 0, FALSE, FALSE},
	{cmd_delete_char_back, GDK_KEY_h, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_del_word_left, GDK_KEY_w, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_indent_ins, GDK_KEY_t, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_unindent_ins, GDK_KEY_d, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_copy_char_from_below, GDK_KEY_e, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_copy_char_from_above, GDK_KEY_y, 0, GDK_CONTROL_MASK, 0, FALSE, FALSE},
	{cmd_paste_before, GDK_KEY_r, 0, GDK_CONTROL_MASK, 0, TRUE, FALSE},

	ARROW_MOTIONS

	{NULL, 0, 0, 0, 0, FALSE, FALSE}
};

/******************************************************************************/

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


/* is the current keypress the first character of a 2-keypress command? */
static gboolean is_cmdpart(GSList *kpl, CmdDef *cmds)
{
	gint i;
	KeyPress *curr = g_slist_nth_data(kpl, 0);

	for (i = 0; cmds[i].cmd != NULL; i++)
	{
		CmdDef *cmd = &cmds[i];
		if ((cmd->key2 != 0 || cmd->param) && key_equals(curr, cmd->key1, cmd->modif1))
			return TRUE;
	}

	return FALSE;
}

static CmdDef *get_cmd_to_run(GSList *kpl, CmdDef *cmds, gboolean have_selection)
{
	gint i;
	KeyPress *curr = g_slist_nth_data(kpl, 0);
	KeyPress *prev = g_slist_nth_data(kpl, 1);
	GSList *below = g_slist_next(kpl);
	ViMode mode = get_vi_mode();

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
					((cmd->needs_selection && have_selection) || !cmd->needs_selection) &&
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
					((cmd->needs_selection && have_selection) || !cmd->needs_selection) &&
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
			((cmd->needs_selection && have_selection) || !cmd->needs_selection) &&
			key_equals(curr, cmd->key1, cmd->modif1))
		{
			// now solve some quirks manually
			if (curr->key == GDK_KEY_0 && !IS_INSERT(mode))
			{
				// 0 jumps to the beginning of line only when not preceded
				// by another number in which case we want to add it to the accumulator
				if (prev == NULL || !kp_isdigit(prev))
					return cmd;
			}
			else if (curr->key == GDK_KEY_percent && !IS_INSERT(mode))
			{
				// % when preceded by a number jumps to N% of the file, otherwise
				// % goes to matching brace
				Cmd c = cmd_goto_matching_brace;
				gint val = kpl_get_int(below, NULL);
				if (val != -1)
					c = cmd_goto_doc_percentage;
				if (cmd->cmd == c)
					return cmd;
			}
			else if (prev && prev->key == GDK_KEY_g)
			{
				// takes care of operator commands like g~, gu, gU where we
				// have no selection yet so the 2-letter command isn't found
				// above and a corresponding 1-letter command ~, u, U exists and
				// would be used instead of waiting for the full command
			}
			else if (is_cmdpart(kpl, text_object_cmds) &&
					get_cmd_to_run(below, operator_cmds, TRUE))
			{
				// if we received "a" or "i", we have to check if there's not
				// an operator command below because these can be part of
				// text object commands (like a<) and in this case we don't
				// want to have "a" or "i" executed yet
			}
			else
				return cmd;
		}
	}

	return NULL;
}


static void perform_cmd(CmdDef *def, ScintillaObject *sci, CmdContext *ctx, GSList *kpl)
{
	GSList *top;
	gint num;
	gint cmd_len = 0;
	gboolean num_present;
	CmdParams param;
	gint orig_pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
	gint sel_start, sel_len;

	if (def->key1 != 0)
		cmd_len++;
	if (def->key2 != 0)
		cmd_len++;
	if (def->param)
		cmd_len++;
	top = g_slist_nth(kpl, cmd_len);
	num = kpl_get_int(top, &top);
	num_present = num != -1;

	sel_start = SSM(sci, SCI_GETSELECTIONSTART, 0, 0);
	sel_len = SSM(sci, SCI_GETSELECTIONEND, 0, 0) - sel_start;
	init_cmd_params(&param, sci,
		num_present ? num : 1, num_present, kpl, FALSE,
		sel_start, sel_len);

	SSM(sci, SCI_BEGINUNDOACTION, 0, 0);

//	if (def->cmd != cmd_undo && def->cmd != cmd_redo)
//		SSM(sci, SCI_ADDUNDOACTION, param.pos, UNDO_MAY_COALESCE);

	def->cmd(ctx, &param);

	if (IS_COMMAND(get_vi_mode()))
	{
		gboolean is_text_object_cmd = is_in_cmd_group(text_object_cmds, def);
		if (is_text_object_cmd ||is_in_cmd_group(movement_cmds, def))
		{
			def = get_cmd_to_run(top, operator_cmds, TRUE);
			if (def)
			{
				gint new_pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);

				SET_POS(sci, orig_pos, FALSE);

				if (is_text_object_cmd)
				{
					sel_start = param.sel_start;
					sel_len = param.sel_len;
				}
				else
				{
					sel_start = MIN(new_pos, orig_pos);
					sel_len = ABS(new_pos - orig_pos);
				}
				init_cmd_params(&param, sci,
					1, FALSE, top, TRUE,
					sel_start, sel_len);

				def->cmd(ctx, &param);
			}
		}
	}

	/* mode could have changed after performing command */
	if (IS_COMMAND(get_vi_mode()))
		clamp_cursor_pos(sci);

	SSM(sci, SCI_ENDUNDOACTION, 0, 0);
}


static gboolean perform_repeat_cmd(ScintillaObject *sci, CmdContext *ctx, GSList *kpl,
	GSList *prev_kpl)
{
	GSList *top = g_slist_next(kpl);  // get behind "."
	gint num = kpl_get_int(top, NULL);
	CmdDef *def;
	gint i;

	def = get_cmd_to_run(prev_kpl, cmd_mode_cmds, FALSE);
	if (!def)
		return FALSE;

	num = num == -1 ? 1 : num;
	for (i = 0; i < num; i++)
		perform_cmd(def, sci, ctx, prev_kpl);

	return TRUE;
}


static gboolean process_event_mode(CmdDef *cmds, ScintillaObject *sci, CmdContext *ctx,
	GSList *kpl, GSList *prev_kpl, gboolean *is_repeat, gboolean *consumed)
{
	gboolean have_selection =
		SSM(sci, SCI_GETSELECTIONEND, 0, 0) - SSM(sci, SCI_GETSELECTIONSTART, 0, 0) > 0;
	CmdDef *def = get_cmd_to_run(kpl, cmds, have_selection);

	*consumed = is_cmdpart(kpl, cmds);

	if (!def)
		return FALSE;

	if (is_repeat != NULL)
	{
		*is_repeat = def->cmd == cmd_repeat_last_command;
		if (*is_repeat)
		{
			if (!perform_repeat_cmd(sci, ctx, kpl, prev_kpl))
				return FALSE;

			*consumed = TRUE;
			return TRUE;
		}
	}

	perform_cmd(def, sci, ctx, kpl);

	*consumed = TRUE;
	return TRUE;
}

gboolean process_event_cmd_mode(ScintillaObject *sci, CmdContext *ctx, GSList *kpl,
	GSList *prev_kpl, gboolean *is_repeat, gboolean *consumed)
{
	return process_event_mode(cmd_mode_cmds, sci, ctx, kpl, prev_kpl, is_repeat, consumed);
}

gboolean process_event_vis_mode(ScintillaObject *sci, CmdContext *ctx, GSList *kpl, gboolean *consumed)
{
	return process_event_mode(vis_mode_cmds, sci, ctx, kpl, NULL, NULL, consumed);
}

gboolean process_event_ins_mode(ScintillaObject *sci, CmdContext *ctx, GSList *kpl, gboolean *consumed)
{
	return process_event_mode(ins_mode_cmds, sci, ctx, kpl, NULL, NULL, consumed);
}
