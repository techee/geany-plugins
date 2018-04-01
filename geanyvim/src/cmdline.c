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


static void perform_normal_cmdline_cmd(CmdContext *ctx, const gchar *cmd)
{
	CmdlineCmdParams params;
	gchar **parts, **part;
	gchar *first = NULL;
	gchar *second = NULL;
	gint i;

	if (strlen(cmd) < 1)
		return;

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
			perform_search(ctx, ctx->num, FALSE);
			break;
	}
}
