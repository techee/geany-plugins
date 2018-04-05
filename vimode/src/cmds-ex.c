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

#include "cmds-ex.h"
#include "utils.h"

#include <string.h>
#include <ctype.h>


typedef struct
{
	gboolean force;
} ExCmdParams;

typedef void (*ExCmd)(CmdContext *c, ExCmdParams *p);

typedef struct {
	ExCmd cmd;
	const gchar *name;
} ExCmdDef;


static void cmd_save(CmdContext *c, ExCmdParams *p)
{
	c->cb->on_save(p->force);
}


static void cmd_save_all(CmdContext *c, ExCmdParams *p)
{
	c->cb->on_save_all(p->force);
}


static void cmd_quit(CmdContext *c, ExCmdParams *p)
{
	c->cb->on_quit(p->force);
}


static void cmd_save_quit(CmdContext *c, ExCmdParams *p)
{
	if (c->cb->on_save(p->force))
		c->cb->on_quit(p->force);
}


static void cmd_save_all_quit(CmdContext *c, ExCmdParams *p)
{
	if (c->cb->on_save_all(p->force))
		c->cb->on_quit(p->force);
}

static void perform_substitute(CmdContext *ctx, const gchar *cmd, gint from, gint to)
{
	gchar *copy = g_strdup(cmd);
	gchar *p = copy;
	gchar *pattern = NULL;
	gchar *repl = NULL;
	gchar *flags = NULL;

	while (*p)
	{
		if (*p == '/' && *(p-1) != '\\')
		{
			if (!pattern)
				pattern = p+1;
			else if (!repl)
				repl = p+1;
			else if (!flags)
				flags = p+1;
			*p = '\0';
		}
		p++;
	}


	if (pattern && repl)
	{
		struct Sci_TextToFind ttf;
		gint find_flags = SCFIND_REGEXP | SCFIND_MATCHCASE;
		GString *s = g_string_new(pattern);
		gboolean all = flags && strstr(flags, "g") != NULL;

		while (TRUE)
		{
			p = strstr(s->str, "\\c");
			if (!p)
				break;
			g_string_erase(s, p - s->str, 2);
			find_flags &= ~SCFIND_MATCHCASE;
		}

		ttf.lpstrText = s->str;
		ttf.chrg.cpMin = SSM(ctx->sci, SCI_POSITIONFROMLINE, from, 0);
		ttf.chrg.cpMax = SSM(ctx->sci, SCI_GETLINEENDPOSITION, to, 0);
		while (SSM(ctx->sci, SCI_FINDTEXT, find_flags, (sptr_t)&ttf) != -1)
		{
			SSM(ctx->sci, SCI_SETTARGETSTART, ttf.chrgText.cpMin, 0);
			SSM(ctx->sci, SCI_SETTARGETEND, ttf.chrgText.cpMax, 0);
			SSM(ctx->sci, SCI_REPLACETARGET, -1, (sptr_t)repl);

			if (!all)
				break;
		}

		g_string_free(s, TRUE);
	}

	g_free(copy);
}

/******************************************************************************/

ExCmdDef ex_cmds[] = {
	{cmd_save, "w"},
	{cmd_save, "write"},
	{cmd_save, "up"},
	{cmd_save, "update"},

	{cmd_save_all, "wall"},

	{cmd_quit, "q"},
	{cmd_quit, "quit"},
	{cmd_quit, "quita"},
	{cmd_quit, "quitall"},
	{cmd_quit, "qa"},
	{cmd_quit, "qall"},
	{cmd_quit, "cq"},
	{cmd_quit, "cquit"},
	
	{cmd_save_quit, "wq"},
	{cmd_save_quit, "x"},
	{cmd_save_quit, "xit"},
	{cmd_save_quit, "exi"},
	{cmd_save_quit, "exit"},

	{cmd_save_all_quit, "xa"},
	{cmd_save_all_quit, "xall"},
	{cmd_save_all_quit, "wqa"},
	{cmd_save_all_quit, "wqall"},
	{cmd_save_all_quit, "x"},
	{cmd_save_all_quit, "xit"},

	{NULL, NULL}
};

/******************************************************************************/

typedef enum
{
	TK_END,
	TK_EOL,
	TK_ERROR,

	TK_PLUS,
	TK_MINUS,
	TK_NUMBER,
	TK_COLON,
	TK_SEMICOLON,
	TK_DOT,
	TK_DOLLAR,
	TK_VISUAL_START,
	TK_VISUAL_END,
	TK_PATTERN,

	TK_STAR,
	TK_PERCENT,
} TokenType;


typedef struct
{
	TokenType type;
	gint num;
	gchar *str;
} Token;


static void init_tk(Token *tk, TokenType type, gint num, gchar *str)
{
	tk->type = type;
	tk->num = num;
	g_free(tk->str);
	tk->str = str;
}


static TokenType get_simple_token_type(gchar c)
{
	switch (c)
	{
		case '+':
			return TK_PLUS;
		case '-':
			return TK_MINUS;
		case ';':
			return TK_SEMICOLON;
		case ',':
			return TK_COLON;
		case '.':
			return TK_DOT;
		case '$':
			return TK_DOLLAR;
		case '*':
			return TK_STAR;
		case '%':
			return TK_PERCENT;
		default:
			break;
	}
	return TK_ERROR;
}


static void next_token(const gchar **p, Token *tk)
{
	TokenType type;

	while (isspace(**p))
		(*p)++;

	if (**p == '\0')
	{
		init_tk(tk, TK_EOL, 0, NULL);
		return;
	}

	if (isdigit(**p))
	{
		gint num = 0;
		while (isdigit(**p))
		{
			num = 10 * num + (**p - '0');
			(*p)++;
		}
		init_tk(tk, TK_NUMBER, num, NULL);
		return;
	}

	type = get_simple_token_type(**p);
	if (type != TK_ERROR)
	{
		(*p)++;
		init_tk(tk, type, 0, NULL);
		return;
	}

	if (**p == '/' || **p == '?')
	{
		gchar c = **p;
		gchar begin[2] = {c, '\0'};
		GString *s = g_string_new(begin);
		(*p)++;
		while (**p != c && **p != '\0')
		{
			g_string_append_c(s, **p);
			(*p)++;
		}
		if (**p == c)
			(*p)++;
		init_tk(tk, TK_PATTERN, 0, s->str);
		g_string_free(s, FALSE);
		return ;
	}

	if (**p == '\'')
	{
		(*p)++;
		if (**p == '<')
		{
			(*p)++;
			init_tk(tk, TK_VISUAL_START, 0, NULL);
			return;
		}
		else if (**p == '>')
		{
			(*p)++;
			init_tk(tk, TK_VISUAL_END, 0, NULL);
			return;
		}
		else
		{
			init_tk(tk, TK_ERROR, 0, NULL);
			return;
		}
	}

	init_tk(tk, TK_END, 0, NULL);
	return;
}


typedef enum
{
	ST_START,
	ST_AFTER_NUMBER,
	ST_BEFORE_END
} State;


static gboolean parse_ex_range(const gchar **p, CmdContext *ctx, gint *from, gint *to)
{
	Token *tk = g_new0(Token, 1);
	State state = ST_START;
	gint num = 0;
	gboolean neg = FALSE;
	gint count = 0;
	gboolean success = TRUE;

	next_token(p, tk);

	while (TRUE)
	{
		if (state == ST_START)
		{
			switch (tk->type)
			{
				case TK_PLUS:
					state = ST_AFTER_NUMBER;
					break;
				case TK_MINUS:
					state = ST_AFTER_NUMBER;
					neg = !neg;
					break;
				case TK_NUMBER:
					state = ST_AFTER_NUMBER;
					num = tk->num - 1;
					break;
				case TK_DOT:
					state = ST_AFTER_NUMBER;
					num = GET_CUR_LINE(ctx->sci);
					break;
				case TK_DOLLAR:
					state = ST_AFTER_NUMBER;
					num = SSM(ctx->sci, SCI_GETLINECOUNT, 0, 0) - 1;
					break;
				case TK_VISUAL_START:
				{
					state = ST_AFTER_NUMBER;
					gint min = MIN(ctx->sel_anchor, SSM(ctx->sci, SCI_GETCURRENTPOS, 0, 0));
					num = SSM(ctx->sci, SCI_LINEFROMPOSITION, min, 0);
					break;
				}
				case TK_VISUAL_END:
				{
					state = ST_AFTER_NUMBER;
					gint max = MAX(ctx->sel_anchor, SSM(ctx->sci, SCI_GETCURRENTPOS, 0, 0));
					num = SSM(ctx->sci, SCI_LINEFROMPOSITION, max, 0);
					break;
				}
				case TK_PATTERN:
				{
					gint pos = perform_search(ctx->sci, tk->str, ctx->num, FALSE);
					num = SSM(ctx->sci, SCI_LINEFROMPOSITION, pos, 0);
					state = ST_AFTER_NUMBER;
					break;
				}

				case TK_PERCENT:
					state = ST_BEFORE_END;
					*to = 0;
					num = SSM(ctx->sci, SCI_GETLINECOUNT, 0, 0) - 1;
					count++;
					break;
				case TK_STAR:
				{
					gint pos = MIN(ctx->sel_anchor, SSM(ctx->sci, SCI_GETCURRENTPOS, 0, 0));
					*to = SSM(ctx->sci, SCI_LINEFROMPOSITION, pos, 0);
					pos = MAX(ctx->sel_anchor, SSM(ctx->sci, SCI_GETCURRENTPOS, 0, 0));
					num = SSM(ctx->sci, SCI_LINEFROMPOSITION, pos, 0);
					state = ST_BEFORE_END;
					count++;
					break;
				}

				case TK_SEMICOLON:
				case TK_COLON:
					//we don't have number yet, ignore
					break;
				default:
					goto finish;
			}
		}
		else if (state == ST_AFTER_NUMBER || state == ST_BEFORE_END)
		{
			if (tk->type == TK_SEMICOLON || tk->type == TK_COLON ||
				tk->type == TK_END || tk->type == TK_EOL)
			{
				num = MAX(num, 0);
				num = MIN(num, SSM(ctx->sci, SCI_GETLINECOUNT, 0, 0) - 1);
				if (tk->type == TK_SEMICOLON || tk->type == TK_EOL)
					goto_nonempty(ctx->sci, num, TRUE);

				*from = *to;
				*to = num;

				neg = FALSE;
				num = 0;
				count++;
				state = ST_START;
			}
			else if (state == ST_AFTER_NUMBER)
			{
				switch (tk->type)
				{
					case TK_PLUS:
						break;
					case TK_MINUS:
						neg = !neg;
						break;
					case TK_NUMBER:
						num += neg ? -tk->num : tk->num;
						neg = FALSE;
						break;
					default:
						goto finish;
				}
			}
			else
				goto finish;
		}
		next_token(p, tk);
	}

finish:
	if (tk->type != TK_EOL && tk->type != TK_END)
		success = FALSE;
	g_free(tk->str);
	g_free(tk);
	if (count == 0)
		*from = *to = GET_CUR_LINE(ctx->sci);
	else if (count == 1)
		*from = *to;
	return success;
}

/******************************************************************************/

static void perform_simple_ex_cmd(CmdContext *ctx, const gchar *cmd)
{
	gchar **parts, **part;
	gchar *first = NULL;
	gchar *second = NULL;
	gint from = 0;
	gint to = 0;

	if (strlen(cmd) < 1)
		return;

	if (!parse_ex_range(&cmd, ctx, &from, &to))
		return;

	if (g_str_has_prefix(cmd, "s/") || g_str_has_prefix(cmd, "substitute/"))
	{
		perform_substitute(ctx, cmd, from, to);
		return;
	}

	parts = g_strsplit(cmd, " ", 0);

	for (part = parts; *part; part++)
	{
		if (strlen(*part) != 0)
		{
			if (!first)
				first = *part;
			else if (!second)
				second = *part;
		}
	}

	if (first)
	{
		ExCmdParams params;
		gint i;

		params.force = FALSE;
		if (first[strlen(first)-1] == '!')
		{
			first[strlen(first)-1] = '\0';
			params.force = TRUE;
		}

		for (i = 0; ex_cmds[i].cmd != NULL; i++)
		{
			ExCmdDef *def = &ex_cmds[i];
			if (strcmp(def->name, first) == 0)
			{
				def->cmd(ctx, &params);
				break;
			}
		}
	}

	g_strfreev(parts);
}


void perform_ex_cmd(CmdContext *ctx, const gchar *cmd)
{
	guint len = strlen(cmd);

	if (cmd == NULL || len < 2)
		return;

	switch (cmd[0])
	{
		case ':':
			perform_simple_ex_cmd(ctx, cmd + 1);
			break;
		case '/':
		case '?':
		{
			gint pos;
			if (len == 1)
			{
				if (ctx->search_text && strlen(ctx->search_text) > 1)
					ctx->search_text[0] = cmd[0];
			}
			else
			{
				g_free(ctx->search_text);
				ctx->search_text = g_strdup(cmd);
			}
			pos = perform_search(ctx->sci, ctx->search_text, ctx->num, FALSE);
			if (pos >= 0)
				SET_POS(ctx->sci, pos, TRUE);
			break;
		}
	}
}
