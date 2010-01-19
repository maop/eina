/*
 * plugins/coverplus/infolder-sync.c
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

#define GEL_DOMAIN "Eina::Plugin::CoverPlus::Infolder-Sync"

#include <gel/gel-io.h>
#include "infolder.h"

static gchar *coverplus_infolder_regex_str[] = {
	".*front.*\\.(jpe?g|png|gif)$",
	".*cover.*\\.(jpe?g|png|gif)$",
	".*folder.*\\.(jpe?g|png|gif)$",
	".*\\.(jpe?g|png|gif)$",
	NULL
};

struct _CoverPlusInfolder {
	GRegex       *regexes[4]; // Keep in sync with size of coverplus_infolder_regex_str
	GCancellable *cancellable;
	gint          score;
};

CoverPlusInfolder *
coverplus_infolder_new(EinaPlugin *plugin, GError **error)
{
	CoverPlusInfolder *self = g_new0(CoverPlusInfolder, 1);

	// Compile regexes
	gint i;
	for (i = 0; coverplus_infolder_regex_str[i] != NULL; i++)
	{
		self->regexes[i] = g_regex_new(coverplus_infolder_regex_str[i],
			G_REGEX_CASELESS|G_REGEX_DOTALL|G_REGEX_DOLLAR_ENDONLY|G_REGEX_OPTIMIZE|G_REGEX_NO_AUTO_CAPTURE,
			0, error);
		if (!self->regexes[i])
		{
			coverplus_infolder_destroy(self);
			return NULL;
		}
	}

	return self;
}

void
coverplus_infolder_destroy(CoverPlusInfolder *self)
{
	// Free regexes
	gint i;
	for (i = 0; coverplus_infolder_regex_str[i] != NULL; i++)
		if (self->regexes[i])
			g_regex_unref(self->regexes[i]);

	// Bye
	g_free(self);
}

void
coverplus_infolder_art_search_cb(Art *art, ArtSearch *search, CoverPlusInfolder *self)
{
	LomoStream *stream = art_search_get_stream(search);
	const gchar *uri = lomo_stream_get_tag(stream, LOMO_TAG_URI);

	// Check is stream is local
	gchar *scheme =  g_uri_parse_scheme(uri);
	if (!g_str_equal(scheme, "file"))
	{
		gel_warn("sync search using coverplus-infolder only works in local files");
		g_free(scheme);
		art_report_failure(art, search);
		return;
	}
	g_free(scheme);

	gchar *baseuri = g_path_get_dirname(uri);

	// Try to get a list of folder contents
	GError *error = NULL;
	gchar *dirname = g_filename_from_uri(baseuri, NULL, NULL);
	g_free(baseuri);

	GList *children = gel_dir_read(dirname, FALSE, &error);
	if (error)
	{
		gel_warn("Error reading %s: %s", dirname, error->message);
		g_free(dirname);
		g_error_free(error);
		art_report_failure(art, search);
		return;
	}

	GList *iter = children;
	gchar *winner = NULL;
	gint score = G_MAXINT;
	while (iter)
	{
		gint i;
		for (i = 0; coverplus_infolder_regex_str[i] != NULL; i++)
		{
			if (self->regexes[i] && g_regex_match(self->regexes[i], (gchar *) iter->data, 0, NULL) && (score > i))
			{
				winner = iter->data;
				score = i;
			}
		}
		iter = iter->next;
	}

	if (score < G_MAXINT)
	{	
		gchar *cover_pathname = g_build_filename(dirname, winner, NULL);
		GdkPixbuf *pb = gdk_pixbuf_new_from_file(cover_pathname, NULL);
		if (pb)
			art_report_success(art, search, pb);
		else
			art_report_failure(art, search);
		g_free(cover_pathname);
	}
	else
	{
		art_report_failure(art, search);
	}

	// Free used data
	gel_list_deep_free(children, g_free);
	g_free(dirname);
}

