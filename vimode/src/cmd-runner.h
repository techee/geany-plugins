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

#ifndef __VIMODE_CMD_RUNNER_H__
#define __VIMODE_CMD_RUNNER_H__

#include "context.h"

gboolean cmd_perform_kpl_cmd(CmdContext *ctx, GSList *kpl,
	GSList *prev_kpl, gboolean *is_repeat, gboolean *consumed);
gboolean cmd_perform_kpl_vis(CmdContext *ctx, GSList *kpl, gboolean *consumed);
gboolean cmd_perform_kpl_ins(CmdContext *ctx, GSList *kpl, gboolean *consumed);

#endif
