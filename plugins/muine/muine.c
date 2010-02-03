/*
 * plugins/muine/muine.c
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

#define GEL_DOMAIN "Eina::Plugin::Muine"
#define GEL_PLUGIN_DATA_TYPE EinaMuine
#include <eina/eina-plugin.h>
#include <plugins/adb/adb.h>

#define DEFAULT_SIZE 64
#define AUTO_REFRESH 0

enum {
	EINA_MUINE_NO_ERROR = 0,
	EINA_MUINE_ERROR_UI_NOT_FOUND,
	EINA_MUINE_ERROR_MISSING_OBJECTS
};

// Sync with UI
enum {
	EINA_MUINE_MODE_ALBUM  = 0,
	EINA_MUINE_MODE_ARTIST = 1
};

enum {
	COMBO_COLUMN_ICON,
	COMBO_COLUMN_MARKUP,

	COMBO_COLUMN_ID,
	COMBO_COLUMN_SEARCH
};

struct _EinaMuine {
	EinaObj  parent;

	GtkWidget *dock;

	GtkTreeView  *view;
	GtkListStore *model;

	GdkPixbuf *default_icon;
	guint refresh_id;
};
typedef struct _EinaMuine EinaMuine;

GEL_AUTO_QUARK_FUNC(muine)

static void
muine_model_refresh(EinaMuine *self);
static void
muine_model_clear(EinaMuine *self);
#if AUTO_REFRESH
static void
muine_schedule_refresh(EinaMuine *self);
#endif
static void
row_activated_cb(GtkWidget *w, GtkTreePath *path, GtkTreeViewColumn *column, EinaMuine *self);
static void
action_activate_cb(GtkAction *action, EinaMuine *self);
static void
search_cb(Art *art, ArtSearch *search, EinaMuine *self);

#define muine_get_mode(self) EINA_MUINE_MODE_ALBUM

static gboolean
muine_init(EinaMuine *self, GError **error)
{
	self->dock  = eina_obj_get_typed(self, GTK_WIDGET, "main-widget");
	self->view  = eina_obj_get_typed(self, GTK_TREE_VIEW, "list-view");
	self->model = eina_obj_get_typed(self, GTK_LIST_STORE, "model");

	GError *err = NULL;
	gchar *icon_path = NULL;
	if (!(icon_path = gel_resource_locate(GEL_RESOURCE_IMAGE, "eina.svg")) ||
	    !(self->default_icon = gdk_pixbuf_new_from_file_at_scale(icon_path, DEFAULT_SIZE, DEFAULT_SIZE, TRUE, &err)))
	{
		if (err)
		{
			gel_warn(N_("Cannot load resource %s: %s"), icon_path, err->message);
			g_error_free(err);
		}

		if (!icon_path)
			gel_warn(N_("Cannot locate resource %s"), "eina.svg");
		else
			g_free(icon_path);
	}

	if (!self->dock || !self->view || !self->model)
	{
		g_set_error(error, muine_quark(), EINA_MUINE_ERROR_MISSING_OBJECTS,
			N_("Missing widgets D:%p V:%p M:%p"),
			self->dock, self->view, self->model);
		return FALSE;
	}
	g_object_set(eina_obj_get_object(self, "markup-renderer"), "yalign", 0.0f, NULL);
	g_signal_connect(self->view, "row-activated", (GCallback) row_activated_cb, self);

	#if AUTO_REFRESH
	LomoPlayer *lomo = eina_obj_get_lomo(self);
	g_signal_connect_swapped(lomo, "insert", (GCallback) muine_schedule_refresh, self);
	#endif

	gint i;
	gchar *actions[] = { "play-action", "queue-action", NULL };
	for (i = 0; actions[i] != NULL; i++)
	{
		GObject *a = eina_obj_get_object(self, actions[i]);
		g_signal_connect(a, "activate", (GCallback) action_activate_cb, self);
	}

	muine_model_refresh(self);

	gtk_widget_unparent(self->dock);
	gtk_widget_show(self->dock);

	EinaDock *dock = eina_obj_get_dock(self);
	eina_dock_add_widget(dock, "muine", gtk_image_new_from_stock(GTK_STOCK_CDROM, GTK_ICON_SIZE_SMALL_TOOLBAR), self->dock);
	return TRUE;
}

static gboolean
muine_destroy(EinaMuine *self, GError **error)
{
	if (!self || !self->dock)
		return TRUE;

	muine_model_clear(self);

	EinaDock *dock = eina_obj_get_dock(self);
	eina_dock_remove_widget(dock, "muine");

	gel_free_and_invalidate(self->dock, NULL, g_object_unref);
	g_object_unref(self->dock);
	self->dock = FALSE;

	return TRUE;
}

static void
muine_model_clear(EinaMuine *self)
{
	GtkTreeIter iter;
	if (self->model && gtk_tree_model_get_iter_first((GtkTreeModel *) self->model, &iter))
	{
		Art *art = eina_obj_get_art(self);
		ArtSearch *search = NULL;
		do {
			gtk_tree_model_get((GtkTreeModel *) self->model, &iter,
				COMBO_COLUMN_SEARCH, &search,
				-1);

			if (search)
				art_cancel(art, search);
		} while (gtk_tree_model_iter_next((GtkTreeModel *) self->model, &iter));
	}
	gtk_list_store_clear(self->model);
}

#if AUTO_REFRESH
static gboolean
muine_model_refresh_wrapper(EinaMuine *self)
{
	muine_model_refresh(self);
	self->refresh_id = 0;
	return FALSE;
}

static void
muine_schedule_refresh(EinaMuine *self)
{
	if (self->refresh_id)
		g_source_remove(self->refresh_id);
	self->refresh_id = g_timeout_add_seconds(10, (GSourceFunc) muine_model_refresh_wrapper, self);
}
#endif

// Very rudimental refresh
static void
muine_model_refresh(EinaMuine *self)
{
	Adb *adb = eina_obj_get_adb(self);
	gint mode = muine_get_mode(self);
	gchar *q = NULL;
	gchar *markup_fmt = NULL;
	switch (mode)
	{
	case EINA_MUINE_MODE_ALBUM:
		q = "select count(*) as count,artist,album from fast_meta group by(lower(album)) order by album DESC";
		markup_fmt = "<big><b>%s</b></big>\n%s <span size=\"small\" weight=\"light\">(%d streams)</span>";
		break;
	case EINA_MUINE_MODE_ARTIST:
		q = "select count(*) as count,artist,NULL from fast_meta group by(lower(artist)) order by artist DESC";
		markup_fmt = "<big><b>%s</b></big>\n<span size=\"small\" weight=\"light\">(%d streams)</span>";
		break;
	default:
		g_warning(N_("Unknow mode"));
		return;
	}

	AdbResult *r = adb_query(adb, q, NULL);
	if (!r)
	{
		gel_warn("Query failed");
		return;
	}
	muine_model_clear(self);

	gchar *db_album = NULL, *db_artist = NULL;
	gchar *album = NULL, *artist = NULL;
	gint   count;
	while (adb_result_step(r))
	{
		if (!adb_result_get(r,
			0, G_TYPE_UINT, &count,
			1, G_TYPE_STRING, &db_artist,
			2, G_TYPE_STRING, &db_album,
			-1))
		{
			gel_warn("Cannot get row results");
			continue;
		}

		LomoStream *fake_stream = NULL;
		if (db_artist || db_album)
			fake_stream = lomo_stream_new("file:///dev/null");

		if (db_artist)
		{
			artist = g_markup_escape_text(db_artist, -1);
			lomo_stream_set_tag(fake_stream, LOMO_TAG_ARTIST, db_artist);
		}
		if (db_album)
		{
			album  = g_markup_escape_text(db_album,  -1);
			lomo_stream_set_tag(fake_stream, LOMO_TAG_ALBUM,  db_album);
		}

		ArtSearch *search = art_search(eina_obj_get_art(self), fake_stream, (ArtFunc) search_cb, self);

		gchar *markup;
		switch (mode)
		{
		case EINA_MUINE_MODE_ALBUM:
			markup = g_strdup_printf(markup_fmt, album, artist, count);
			break;
		case EINA_MUINE_MODE_ARTIST:
			markup = g_strdup_printf(markup_fmt, artist, count);
			break;
		}
		gtk_list_store_insert_with_values(self->model, NULL, 0,
			COMBO_COLUMN_MARKUP, markup,
			COMBO_COLUMN_ID,     (mode == EINA_MUINE_MODE_ALBUM) ? db_album : db_artist,
			COMBO_COLUMN_SEARCH, search,
			COMBO_COLUMN_ICON,   self->default_icon,
			-1);
		g_free(markup);
		gel_free_and_invalidate(artist, NULL, g_free);
		gel_free_and_invalidate(album,  NULL, g_free);
	}
	adb_result_free(r);
}

static void
muine_update_icon(EinaMuine *self, ArtSearch *search)
{
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter_first((GtkTreeModel *) self->model, &iter))
	{
		gel_error(N_("Cannot get first iter"));
		return;
	}

	GdkPixbuf *icon = art_search_get_result(search);
	GdkPixbuf *scaled = (icon && GDK_IS_PIXBUF(icon)) ? gdk_pixbuf_scale_simple(icon, DEFAULT_SIZE, DEFAULT_SIZE, GDK_INTERP_NEAREST) : NULL;

	ArtSearch *test;
	do {
		gtk_tree_model_get((GtkTreeModel *) self->model, &iter,
			COMBO_COLUMN_SEARCH, &test,
			-1);
		if (test == search)
		{
			if (scaled)
				gtk_list_store_set(self->model, &iter,
					COMBO_COLUMN_ICON, scaled,
					COMBO_COLUMN_SEARCH, NULL,
					-1);
			else
				gtk_list_store_set(self->model, &iter,
					COMBO_COLUMN_SEARCH, NULL,
					-1);
			return;
		}
	} while (gtk_tree_model_iter_next((GtkTreeModel *) self->model, &iter));
	gel_error("Search NOT found");
}

static GList *
muine_get_uris_from_tree_iter(EinaMuine *self, GtkTreeIter *iter)
{
	gchar *id = NULL;
	gtk_tree_model_get((GtkTreeModel *) self->model, iter,
		COMBO_COLUMN_ID, &id,
		-1);
	
	char *q = NULL;
	switch (muine_get_mode(self))
	{
	case EINA_MUINE_MODE_ALBUM:
		q = "select uri from streams where sid in (select sid from fast_meta where lower(album)=lower('%q'))";
		break;
	case EINA_MUINE_MODE_ARTIST:
		q = "select uri from streams where sid in (select sid from fast_meta where lower(artist)=lower('%q'))";
		break;
	default:
		g_warning(N_("Unknow mode"));
		return NULL;
	}

	Adb *adb = eina_obj_get_adb(self);
	AdbResult *r = adb_query(adb, q, id);
	if (r == NULL)
	{
		gel_warn(N_("Unable to build query '%s'"), q);
		return NULL;
	}

	GList *uris = NULL;
	gchar *uri;
	while (adb_result_step(r))
		if (adb_result_get(r, 0, G_TYPE_STRING, &uri, -1))
			uris = g_list_prepend(uris, uri);
	adb_result_free(r);

	return g_list_reverse(uris);
}

static void
row_activated_cb(GtkWidget *w, GtkTreePath *path, GtkTreeViewColumn *column, EinaMuine *self)
{
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter((GtkTreeModel *) self->model, &iter, path))
	{
		gel_error(N_("Cannot get iter  for model"));
		return;
	}

	GList *uris = muine_get_uris_from_tree_iter(self, &iter);
	if (uris == NULL)
	{
		gel_error(N_("NULL result from ADB"));
		return;
	}

	LomoPlayer *lomo = eina_obj_get_lomo(self);
	lomo_player_clear(lomo);
	lomo_player_append_uri_multi(lomo, uris);

	gel_list_deep_free(uris, g_free);
}

static void
action_activate_cb(GtkAction *action, EinaMuine *self)
{
	const gchar *name = gtk_action_get_name(action);
	gboolean do_clear = FALSE;

	if (g_str_equal(name, "play-action"))
		do_clear = TRUE;
	else if (g_str_equal(name, "queue-action"))
		do_clear = FALSE;
	else
	{
		g_warning(N_("Unknow action %s"), name);
		return;
	}

	GtkTreeSelection *sel = gtk_tree_view_get_selection(self->view);
	if (!sel)
		return;

	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return;

	GList *uris = muine_get_uris_from_tree_iter(self, &iter);
	if (!uris)
		return;

	LomoPlayer *lomo = eina_obj_get_lomo(self);
	if (do_clear)
		lomo_player_clear(lomo);
	lomo_player_append_uri_multi(lomo, uris);
	gel_list_deep_free(uris, g_free);
}

static void
search_cb(Art *art, ArtSearch *search, EinaMuine *self)
{
	muine_update_icon(self, search);
}

G_MODULE_EXPORT gboolean
muine_iface_init(GelApp *app, GelPlugin *plugin, GError **error)
{
	EinaMuine *self;

	self = g_new0(EinaMuine, 1);
	if (!eina_obj_init(EINA_OBJ(self), plugin, "muine", EINA_OBJ_GTK_UI, error))
		return FALSE;
	plugin->data = self;

	if (!muine_init(self, error))
	{
		eina_obj_fini((EinaObj *) self);
		return FALSE;
	}

	return TRUE;
}

G_MODULE_EXPORT gboolean
muine_iface_fini(GelApp *app, GelPlugin *plugin, GError **error)
{
	EinaMuine *self = GEL_PLUGIN_DATA(plugin);

	gboolean ret = muine_destroy(self, error);
	if (ret == FALSE)
		return FALSE;
	
	eina_obj_fini((EinaObj *) self);
	return TRUE;
}

EINA_PLUGIN_SPEC(muine,
	"0.0.1",
	"dock,art,adb,lomo,settings",

	NULL,
	NULL,

	N_("Build playlist from your albums"),
	N_("This plugin allows Eina to work like Muine music player.\n"
	"You can handle your playlists grouped by album. Add once, play anytime"),
	NULL,

	muine_iface_init,
	muine_iface_fini
);

