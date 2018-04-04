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

#include "cmdline.h"
#include "utils.h"

#include <string.h>
#include <ctype.h>


typedef struct
{
	gboolean force;
} CmdlineCmdParams;

typedef void (*CmdlineCmd)(CmdContext *c, CmdlineCmdParams *p);

typedef struct {
	CmdlineCmd cmd;
	const gchar *cmdline;
} CmdlineCmdDef;


static void cmd_save(CmdContext *c, CmdlineCmdParams *p)
{
	c->cb->on_save(p->force);
}


static void cmd_save_all(CmdContext *c, CmdlineCmdParams *p)
{
	c->cb->on_save_all(p->force);
}


static void cmd_quit(CmdContext *c, CmdlineCmdParams *p)
{
	c->cb->on_quit(p->force);
}


static void cmd_save_quit(CmdContext *c, CmdlineCmdParams *p)
{
	if (c->cb->on_save(p->force))
		c->cb->on_quit(p->force);
}


static void cmd_save_all_quit(CmdContext *c, CmdlineCmdParams *p)
{
	if (c->cb->on_save_all(p->force))
		c->cb->on_quit(p->force);
}


CmdlineCmdDef cmdline_cmds[] = {
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

static gint parse_ex_range(const gchar **p, CmdContext *ctx, gint *one, gint *two)
{
	Token *tk = g_new0(Token, 1);
	State state = ST_START;
	gint num = 0;
	gboolean neg = FALSE;
	gint count = 0;

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
					gint pos = perform_search(ctx, ctx->num, FALSE);
					num = SSM(ctx->sci, SCI_LINEFROMPOSITION, pos, 0);
					state = ST_AFTER_NUMBER;
					break;
				}

				case TK_PERCENT:
					state = ST_BEFORE_END;
					*two = 0;
					num = SSM(ctx->sci, SCI_GETLINECOUNT, 0, 0) - 1;
					count++;
					break;
				case TK_STAR:
				{
					gint pos = MIN(ctx->sel_anchor, SSM(ctx->sci, SCI_GETCURRENTPOS, 0, 0));
					*two = SSM(ctx->sci, SCI_LINEFROMPOSITION, pos, 0);
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

				*one = *two;
				*two = num;

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
		count = -1;
	g_free(tk->str);
	g_free(tk);
	return MIN(count, 2);
}


static void perform_normal_cmdline_cmd(CmdContext *ctx, const gchar *cmd)
{
	CmdlineCmdParams params;
	gchar **parts, **part;
	gchar *first = NULL;
	gchar *second = NULL;
	gint i;

	if (strlen(cmd) < 1)
		return;

	gint one = 0;
	gint two = 0;
	gint n = parse_ex_range(&cmd, ctx, &one, &two);
	if (n < 0)
		return;
	else if (n == 0)
		one = two = GET_CUR_LINE(ctx->sci);
	else if (n == 1)
		one = two;
	//printf("range(%d): %d, %d\n", n, one, two);
	//printf("first: %s\n", first);

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

	if (!first)
	{
		g_strfreev(parts);
		return;
	}

	params.force = FALSE;

	if (first[strlen(first)-1] == '!')
	{
		first[strlen(first)-1] = '\0';
		params.force = TRUE;
	}

	for (i = 0; cmdline_cmds[i].cmd != NULL; i++)
	{
		CmdlineCmdDef *def = &cmdline_cmds[i];
		if (strcmp(def->cmdline, first) == 0)
		{
			def->cmd(ctx, &params);
			break;
		}
	}

	g_strfreev(parts);
}


void perform_cmdline_cmd(CmdContext *ctx, const gchar *cmd)
{
	guint len = strlen(cmd);
	if (cmd == NULL || len < 2)
		return;

	switch (cmd[0])
	{
		case ':':
			perform_normal_cmdline_cmd(ctx, cmd + 1);
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
			pos = perform_search(ctx, ctx->num, FALSE);
			if (pos >= 0)
				SET_POS(ctx->sci, pos, TRUE);

			break;
		}
	}
}
