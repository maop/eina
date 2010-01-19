/*
 * lomo/lomo-util.h
 *
 * Copyright (C) 2004-2010 Eina
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LOMO_UTIL_H
#define __LOMO_UTIL_H

#include <gst/gst.h>
#include <lomo/lomo-player.h>

G_BEGIN_DECLS

#define lomo_nanosecs_to_secs(x) ((gint64)(x/1000000000L))
#define lomo_secs_to_nanosecs(x) ((gint64)(x*1000000000L))

gboolean lomo_format_to_gst(LomoFormat format, GstFormat *gst_format);
gboolean lomo_state_to_gst(LomoState state, GstState *gst_state);
gboolean lomo_state_from_gst(GstState state, LomoState *lomo_state);
GstStateChangeReturn lomo_state_change_return_to_gst(GstStateChangeReturn ret);

const gchar *gst_state_to_str(GstState state);
const gchar *gst_state_change_return_to_str(GstStateChangeReturn s);

gchar *lomo_create_uri(gchar *str);

G_END_DECLS

#endif // _LOMO_UTIL_H

