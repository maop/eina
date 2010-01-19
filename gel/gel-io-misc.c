/*
 * gel/gel-io-misc.c
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

#include <gel/gel-io-misc.h>

GFile *
gel_io_file_get_child_for_file_info(GFile *parent, GFileInfo *child_info)
{
	const gchar *name;
	
	if ((name = g_file_info_get_name(child_info)) == NULL)
		return NULL;

	return g_file_get_child(parent, name);
}

