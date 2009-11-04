/*
 * eina/fs.h
 *
 * Copyright (C) 2004-2009 Eina
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

#ifndef __FS_H__
#define __FS_H__

#include <glib.h>
#include <gio/gio.h>
#include <lomo/lomo-player.h>

#if 0
void
eina_fs_load_files_from_uri(LomoPlayer *lomo, gchar *uri);
#endif

// Runs a file chooser dialog with all the stuff (cancel, loading, ...)
// and adds results to lomo handling play or enqueue
void
eina_fs_file_chooser_load_files(LomoPlayer *lomo);

// Checking input by uri or GFile
gboolean
eina_fs_is_supported_extension(gchar *uri);
gboolean
eina_fs_is_supported_file(GFile *uri);

// Sync get children for a (gchar*) uri using GVfs
GList*
eina_fs_uri_get_children(gchar *uri);

GSList*
eina_fs_files_from_uri_strv(gchar **uris);

/*
 * Utility functions to handle on-disk files, paths, dirs, etc...
 */
GList* eina_fs_readdir(gchar *path, gboolean abspath);
GList *eina_fs_recursive_readdir(gchar *path, gboolean abspath);
GList* eina_fs_prepend_dirname(gchar *dirname, GList *list);

gchar* eina_fs_utf8_to_ondisk(gchar *path);
gchar* eina_fs_ondisk_to_utf8(gchar *path); 

gboolean eina_fs_file_test(gchar *utf8_path, GFileTest test);

#define eina_fs_mkdir(pathname,mode) (g_mkdir_with_parents(pathname,mode) == 0)

#endif

