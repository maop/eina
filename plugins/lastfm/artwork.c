/*
 * plugins/lastfm/artwork.c
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

#include "artwork.h"

// ****************************
// Store artwork-subplugin data
// ****************************
struct _LastFMArtwork {
	ArtBackend *backend; // Reference the backend
	GHashTable *ctxs;    // SearchCtx's
	CurlEngine *engine;  // CurlEngine C-class
};

// *********************************
// SearchCtx is a ArtSearch extended
// *********************************
typedef struct {
	LastFMArtwork *self;
	Art          *art;         // Art reference
	ArtSearch    *search;      // Art search
	gint          n;           // sub-search index
	CurlEngine   *engine;      // Inet engine
	CurlQuery    *q;           // This query
} SearchCtx;

// **********************
// Sub-searches prototype
// **********************
typedef void (*SearchFunc)(SearchCtx *ctx);

// ******************
// Internal functions
// ******************
static void lastfm_artwork_search(Art *art, ArtSearch *search, LastFMArtwork *self);
static void lastfm_artwork_cancel(Art *art, ArtSearch *search, LastFMArtwork *self);

static SearchCtx* search_ctx_new   (LastFMArtwork *self, Art *art, ArtSearch *search);
static void       search_ctx_free  (SearchCtx *ctx);
static void       search_ctx_search(SearchCtx *ctx); 
static void       search_ctx_cancel(SearchCtx *ctx); 

static void search_ctx_by_album(SearchCtx *ctx);
// static void search_ctx_by_artist(SearchCtx *ctx);

gchar* search_ctx_parse_as_album(gchar *buffer);
gchar* search_ctx_parse_as_artist(gchar *buffer);

static void curl_engine_finish_cb(CurlEngine *engine, CurlQuery *query, SearchCtx *ctx);
static void curl_engine_cover_cb (CurlEngine *engine, CurlQuery *query, SearchCtx *ctx);

// ******************************************
// Initialize and finalize artwork sub-plugin
// ******************************************
gboolean
lastfm_artwork_init(GelApp *app, EinaPlugin *plugin, GError **error)
{
	LastFMArtwork *self = EINA_PLUGIN_DATA(plugin)->artwork = g_new0(LastFMArtwork, 1);

	if ((self->engine = curl_engine_new()) == NULL)
	{
		gel_warn("Cannit init curl interface");
		g_free(self);
		return FALSE;
	}

	self->ctxs    = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) search_ctx_free);
	self->backend = eina_plugin_add_art_backend(plugin, "lastfm",
		(ArtFunc) lastfm_artwork_search, (ArtFunc) lastfm_artwork_cancel,
		self);

	return TRUE;
}

gboolean
lastfm_artwork_fini(GelApp *app, EinaPlugin *plugin, GError **error)
{
	LastFMArtwork *self = EINA_PLUGIN_DATA(plugin)->artwork;

	g_hash_table_destroy(self->ctxs);
	curl_engine_free(self->engine);

	eina_plugin_remove_art_backend(plugin, self->backend);
	g_free(self);

	return TRUE;
}

// ********************
// ArtBackend functions
// ********************
void
lastfm_artwork_search(Art *art, ArtSearch *search, LastFMArtwork *self)
{
	SearchCtx *ctx = search_ctx_new(self, art, search);
	g_return_if_fail(ctx != NULL);

	g_hash_table_replace(self->ctxs, search, ctx);
	search_ctx_search(ctx);
}

void
lastfm_artwork_cancel(Art *art, ArtSearch *search, LastFMArtwork *self)
{
	SearchCtx *ctx = g_hash_table_lookup(self->ctxs, search);
	g_return_if_fail(ctx);

	gel_warn("Cancel search %p", ctx);

	g_hash_table_remove(self->ctxs, search);
}

// **********************************
// Creating and destroying SearchCtxs
// **********************************
SearchCtx*
search_ctx_new(LastFMArtwork *self, Art *art, ArtSearch *search)
{
	SearchCtx *ctx = g_new0(SearchCtx, 1);

	ctx->self   = self;
	ctx->art    = art;
	ctx->search = search;
	ctx->n      = 0;
	ctx->engine = self->engine;
	ctx->q      = NULL;

	return ctx;
}

void
search_ctx_free(SearchCtx *ctx)
{
	g_return_if_fail(ctx != NULL);

	search_ctx_cancel(ctx);
	g_free(ctx);
}

// ***************************
// Search and cancel functions
// ***************************
void
search_ctx_search(SearchCtx *ctx)
{
	// subsearches
	static SearchFunc subsearches[] = {
		search_ctx_by_album,
		// search_ctx_by_artist,
		// search_ctx_by_single,
		NULL
	};

	// Check current index and launch search
	if (subsearches[ctx->n] == NULL)
	{
		art_report_failure(ctx->art, ctx->search);
		g_hash_table_remove(ctx->self->ctxs, ctx->search);
	}
	else
	{
		subsearches[ctx->n](ctx);
	}
}

void
search_ctx_cancel(SearchCtx *ctx)
{
	if (ctx->q)
	{
		curl_engine_cancel(ctx->engine, ctx->q);
		ctx->q = NULL;
	}
}

void
search_ctx_try_next(SearchCtx *ctx)
{
	ctx->n++;
	search_ctx_search(ctx);
}

gchar*
search_ctx_parse(SearchCtx *ctx, gchar *buffer)
{
	static gpointer parsers[] = {
		search_ctx_parse_as_album,
		NULL,
		search_ctx_parse_as_artist,
		// search_ctx_parse_as_single,
		NULL
	};

	gchar *ret = NULL;
	if (parsers[ctx->n] != NULL)
	{
		gchar* (*parser) (gchar *buffer) = parsers[ctx->n];
		ret = parser(buffer);
	}
	return ret;
}

// ***********
// Subsearches
// ***********
static void
search_ctx_by_album(SearchCtx *ctx)
{
	LomoStream *stream = art_search_get_stream(ctx->search);
	gchar *artist = lomo_stream_get_tag(stream, LOMO_TAG_ARTIST);
	gchar *album  = lomo_stream_get_tag(stream, LOMO_TAG_ALBUM);

	// search_by_album needs artist and album tags
	if (!artist || !album)
	{
		search_ctx_try_next(ctx);
		return;
	}

	// Now control belongs to curl_engine_query_cb / load_contents_async_cb
	gchar *a, *b;
	a = g_uri_escape_string(artist, G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, FALSE);
	b = g_uri_escape_string(album, G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, FALSE);
	gchar *uri = g_strdup_printf("http://www.last.fm/music/%s/%s", a, b);
	g_free(a);
	g_free(b);

	ctx->q = curl_engine_query(ctx->engine, uri, (CurlEngineFinishFunc) curl_engine_finish_cb, ctx);

	g_free(uri);
}

/*
static void
search_ctx_by_artist(SearchCtx *ctx)
{
	LomoStream *stream = art_search_get_stream(ctx->search);
	gchar *artist = lomo_stream_get_tag(stream, LOMO_TAG_ARTIST);

	// search_by_artist needs artist tag
	if (!artist)
	{
		search_ctx_try_next(ctx);
		return;
	}

	// Now control belongs to curl_engine_query_cb / load_contents_async_cb
	gchar *a   = g_uri_escape_string(artist, G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, FALSE);
	gchar *uri = g_strdup_printf("http://www.last.fm/music/%s", a);
	g_free(a);

	ctx->q = curl_engine_query(ctx->engine, uri, (CurlEngineFinishFunc) curl_engine_finish_cb, ctx);

	g_free(uri);
}
*/

// *******
// SubParsers
// *******
gchar *
search_ctx_parse_as_album(gchar *buffer)
{
	gchar *tokens[] = {
		"<span class=\"art\"><img",
		"src=\"",
		NULL
	};

	gint i;
	gchar *p = buffer;
	for (i = 0; tokens[i] != NULL; i++)
	{
		if ((p = strstr(p, tokens[i])) == NULL)
			return NULL;
		p += strlen(tokens[i]) * sizeof(gchar);
	}

	gchar *p2 = strstr(p, "\"");
	if (!p2)
		return NULL;
	p2[0] = '\0';
	
	if (g_str_has_suffix(p, "default_album_mega.png"))
		return NULL;

	return g_strdup(p);
}

gchar *
search_ctx_parse_as_artist(gchar *buffer)
{
	gchar *tokens[] = {
		"id=\"catalogueImage\"",
		"src=\"",
		NULL
	};

	gint i;
	gchar *p = buffer;
	for (i = 0; tokens[i] != NULL; i++)
	{
		if (!p || (p = strstr(p, tokens[i])) == NULL)
			return NULL;
		p += strlen(tokens[i]) * sizeof(gchar);
	}

	gchar *p2 = strstr(p, "\"");
	if (!p2)
		return NULL;
	p2[0] = '\0';
	
	if (g_str_has_suffix(p, "default_album_mega.png"))
		return NULL;

	return g_strdup(p);
}

// ******************
// Callback functions
// ******************
static void
curl_engine_finish_cb(CurlEngine *engine, CurlQuery *query, SearchCtx *ctx)
{
	guint8 *buffer;
	gsize   size;
	GError *error = NULL;

	// Search is finished
	ctx->q = NULL;

	// Jump if there was an error fetching
	if (!curl_query_finish(query, &buffer, &size, &error))
	{
		gchar *uri = curl_query_get_uri(query);
		gel_warn(N_("Cannot fetch uri '%s': %s"), uri, error->message);
		g_free(uri);

		goto curl_engine_finish_cb_fail;
	}

	// Jump if parse failed
	gchar *cover = search_ctx_parse(ctx, (gchar *) buffer);
	if (cover == NULL)
	{
		gel_warn(N_("Parse in stage %d failed for search %p"), ctx->n, ctx);
		goto curl_engine_finish_cb_fail;
	}

	// Ok, fetch the cover
	curl_engine_query(ctx->engine, cover,
		(CurlEngineFinishFunc) curl_engine_cover_cb, ctx);
	g_free(cover);
	return;

curl_engine_finish_cb_fail:
	gel_free_and_invalidate(error,  NULL, g_error_free);
	gel_free_and_invalidate(buffer, NULL, g_free);
	search_ctx_try_next(ctx);
}

static void
curl_engine_cover_cb(CurlEngine *engine, CurlQuery *query, SearchCtx *ctx)
{
	guint8 *buffer;
	gsize   size;
	GError *error = NULL;
	GdkPixbufLoader *loader = NULL;

	ctx->q = NULL;

	// Jump if there was an error fetching
	if (!curl_query_finish(query, &buffer, &size, &error))
	{
		gchar *uri = curl_query_get_uri(query);
		gel_warn("Cannot get uri '%s': %s", uri, error->message);
		g_error_free(error);
		g_free(uri);

		goto curl_engine_cover_cb_fail;
	}

	// Load pixbuf
	loader = gdk_pixbuf_loader_new();
	if (!gdk_pixbuf_loader_write(loader, buffer, size, &error))
	{
		gdk_pixbuf_loader_close(loader, NULL);
		gel_warn("Cannot load to pixbuf: %s", error->message);
		goto curl_engine_cover_cb_fail;
	}
	gdk_pixbuf_loader_close(loader, NULL);

	GdkPixbuf *pb = gdk_pixbuf_loader_get_pixbuf(loader);
	if (!pb)
		goto curl_engine_cover_cb_fail;

	g_object_ref(pb);
	g_object_unref(loader);

	// Report and cleanup
	art_report_success(ctx->art, ctx->search, (gpointer) pb);
	search_ctx_free(ctx);
	return;

curl_engine_cover_cb_fail:
	gel_free_and_invalidate(loader, NULL, g_object_unref);
	gel_free_and_invalidate(error,  NULL, g_error_free);
	gel_free_and_invalidate(buffer, NULL, g_free);
	search_ctx_try_next(ctx);
}

