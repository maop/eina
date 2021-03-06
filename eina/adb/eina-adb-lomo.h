/*
 * eina/adb/eina-adb-lomo.h
 *
 * Copyright (C) 2004-2011 Eina
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

#ifndef __EINA_ADB_LOMO_H__
#define __EINA_ADB_LOMO_H__

#include <eina/adb/eina-adb.h>
#include <lomo/lomo-player.h>

G_BEGIN_DECLS

gint eina_adb_lomo_stream_attach_sid(EinaAdb *adb, LomoStream *stream);
gint eina_adb_lomo_stream_get_sid(EinaAdb *adb, LomoStream *stream);

G_END_DECLS

#endif
