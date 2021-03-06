/*
 * eina/player/eina-player.c
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "eina-player.h"
#include <glib/gi18n.h>
#include <gel/gel-io.h>
#include <gel/gel-ui.h>
#include "eina-seek.h"
#include "eina-cover.h"
#include "eina-cover-image.h"

G_DEFINE_TYPE (EinaPlayer, eina_player, GEL_UI_TYPE_GENERIC)

struct _EinaPlayerPrivate {
	// Props.
	LomoPlayer *lomo;

	EinaSeek  *seek;
	EinaCover *cover;
};

enum {
	PROP_LOMO_PLAYER = 1,
	PROP_DEFAULT_PIXBUF,
};

enum {
	ACTION_ACTIVATED,
	LAST_SIGNAL
};
guint eina_player_signals[LAST_SIGNAL] = { 0 };

static void
player_update_information(EinaPlayer *self);

static void
lomo_all_tags_cb(LomoPlayer *lomo, LomoStream *stream, EinaPlayer *self);

static void
action_activated_cb(GtkAction *action, EinaPlayer *self);

static gboolean
binding_volume_int_to_double_cb(GBinding *binding, const GValue *src, GValue *dst, gpointer data);
static gboolean
binding_volume_double_to_int_cb(GBinding *binding, const GValue *src, GValue *dst, gpointer data);
static gboolean
binding_lomo_state_to_gtk_stock(GBinding *binding, const GValue *src, GValue *dst, EinaPlayer *self);
static gboolean
binding_play_pause_lomo_current_to_sensitive(GBinding *binding, const GValue *src, GValue *dst, EinaPlayer *self);
static gboolean
binding_play_pause_lomo_state_to_action(GBinding *binding, const GValue *src, GValue *dst, EinaPlayer *self);

static void
eina_player_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	switch (property_id)
	{
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
eina_player_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	switch (property_id)
	{
	case PROP_LOMO_PLAYER:
		eina_player_set_lomo_player((EinaPlayer *) object, (LomoPlayer *) g_value_get_object(value));
		break;

	case PROP_DEFAULT_PIXBUF:
		eina_player_set_default_pixbuf((EinaPlayer *) object, (GdkPixbuf *) g_value_get_object(value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
eina_player_dispose (GObject *object)
{
	EinaPlayer *self = EINA_PLAYER(object);
	EinaPlayerPrivate *priv = self->priv;

	gel_free_and_invalidate(priv->lomo, NULL, g_object_unref);

	G_OBJECT_CLASS (eina_player_parent_class)->dispose (object);
}

static void
eina_player_class_init (EinaPlayerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (EinaPlayerPrivate));

	object_class->get_property = eina_player_get_property;
	object_class->set_property = eina_player_set_property;
	object_class->dispose = eina_player_dispose;

	g_object_class_install_property(object_class, PROP_LOMO_PLAYER,
		g_param_spec_object("lomo-player", "lomo-player", "lomo-player",
		LOMO_TYPE_PLAYER, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS
		));

	g_object_class_install_property(object_class, PROP_DEFAULT_PIXBUF,
		g_param_spec_object("default-pixbuf", "default-pixbuf", "default-pixbuf",
		GDK_TYPE_PIXBUF, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS
		));

	eina_player_signals[ACTION_ACTIVATED] =
		g_signal_new ("action-activated",
			G_OBJECT_CLASS_TYPE (object_class),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (EinaPlayerClass, action_activated),
			NULL, NULL,
			gel_marshal_BOOLEAN__OBJECT,
			G_TYPE_BOOLEAN,
			1,
			GTK_TYPE_ACTION);
}

static void
eina_player_init (EinaPlayer *self)
{
	self->priv = (G_TYPE_INSTANCE_GET_PRIVATE ((self), EINA_TYPE_PLAYER, EinaPlayerPrivate));
	gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);
}

GtkWidget*
eina_player_new (void)
{
	gchar *xml_string = NULL;
	gel_io_resources_load_file_contents_or_error(EINA_APP_PATH_DOMAIN "/player/main.ui", &xml_string, NULL);

	EinaPlayer *self = EINA_PLAYER(g_object_new (EINA_TYPE_PLAYER,
		"xml-string", xml_string,
		NULL));
	EinaPlayerPrivate *priv = self->priv;
	g_free(xml_string);

	g_object_set(gel_ui_generic_get_object(self, "stream-title-label"),
		"width-chars", 10,
		NULL);
	g_object_set(gel_ui_generic_get_object(self, "stream-artist-label"),
		"width-chars", 10,
		NULL);

	// Seek widget
	g_object_set(priv->seek = eina_seek_new(),
		"current-label",   gel_ui_generic_get_typed(self, GTK_LABEL, "time-current-label"),
		"remaining-label", gel_ui_generic_get_typed(self, GTK_LABEL, "time-remaining-label"),
		"total-label",     gel_ui_generic_get_typed(self, GTK_LABEL, "time-total-label"),
		NULL);
	gel_ui_container_replace_children(
		gel_ui_generic_get_typed(self, GTK_CONTAINER, "seek-container"),
		GTK_WIDGET(priv->seek));
	gtk_widget_show(GTK_WIDGET(priv->seek));

	// Cover widget
	priv->cover = g_object_new(EINA_TYPE_COVER,
		"renderer", g_object_new(EINA_TYPE_COVER_IMAGE, NULL),
		NULL);
	GtkContainer *cover_container = gel_ui_generic_get_typed(self, GTK_CONTAINER, "cover-container");
	gel_ui_container_replace_children(cover_container, (GtkWidget *) priv->cover);
	g_object_set((GObject *) priv->cover, "hexpand", FALSE, NULL);

	gtk_widget_show(GTK_WIDGET(priv->cover));

	// Actions
	GtkBuilder *builder = gel_ui_generic_get_builder((GelUIGeneric *) self);
	const gchar *const actions[] = {
		"prev-action" ,
		"next-action" ,
		"play-action" ,
		"pause-action",
		"open-action" };
	for (guint i = 0; i < G_N_ELEMENTS(actions); i++)
	{
		GtkAction *action = GTK_ACTION(gtk_builder_get_object(builder, actions[i]));
		if (!action)
		{
			g_warning("Action '%s' not found", actions[i]);
			continue;
		}
		g_signal_connect(action, "activate", (GCallback) action_activated_cb, self);
	}

	return GTK_WIDGET(self);
}

/**
 * eina_player_get_lomo_player:
 * @self: An #EinaPlayer
 *
 * Returns the associated #LomoPlayer
 *
 * Returns: (transfer none): the #LomoPlayer
 */
LomoPlayer *
eina_player_get_lomo_player(EinaPlayer *self)
{
	g_return_val_if_fail(EINA_IS_PLAYER(self), NULL);
	return self->priv->lomo;
}

void
eina_player_set_lomo_player(EinaPlayer *self, LomoPlayer *lomo)
{
	g_return_if_fail(EINA_IS_PLAYER(self));
	g_return_if_fail(LOMO_IS_PLAYER(lomo));

	EinaPlayerPrivate *priv = self->priv;
	g_return_if_fail(!priv->lomo);

	const struct {
		gchar *signal;
		GCallback callback;
		gboolean swapped;
	} callback_defs[] = {
		{ "notify::current", G_CALLBACK(player_update_information), TRUE },
		{ "all-tags",        G_CALLBACK(lomo_all_tags_cb),          FALSE }
	};

	if (priv->lomo)
	{
		for (guint i = 0 ; i < G_N_ELEMENTS(callback_defs); i++)
			g_signal_handlers_disconnect_by_func(priv->lomo, callback_defs[i].callback, self);
		g_object_unref((GObject *) priv->lomo);
	}

	priv->lomo = g_object_ref(lomo);

	for (guint i = 0 ; i < G_N_ELEMENTS(callback_defs); i++)
		if (callback_defs[i].swapped)
			g_signal_connect_swapped(priv->lomo, callback_defs[i].signal, callback_defs[i].callback, self);
		else
			g_signal_connect(priv->lomo, callback_defs[i].signal, callback_defs[i].callback, self);

	GtkBuilder *builder = gel_ui_generic_get_builder(GEL_UI_GENERIC(self));
	struct {
		const gchar *s_prop;
		GObject *dest;
		const gchar *d_prop;
		GBindingFlags flags;
		GBindingTransformFunc src2dst, dst2src;
	} bindings[] = {
		{
			"can-go-previous",
			gel_ui_generic_get_typed(self, G_OBJECT, "prev-button"), "sensitive",
			G_BINDING_SYNC_CREATE,
			NULL, NULL
		},
		{
			"can-go-next",
			gel_ui_generic_get_typed(self, G_OBJECT, "next-button"), "sensitive",
			G_BINDING_SYNC_CREATE,
			NULL, NULL
		},
		{
			"current",
			gel_ui_generic_get_typed(self, G_OBJECT, "play-pause-button"), "sensitive",
			G_BINDING_SYNC_CREATE,
			(GBindingTransformFunc) binding_play_pause_lomo_current_to_sensitive, NULL
		},
		{
			"state",
			gel_ui_generic_get_typed(self, G_OBJECT, "play-pause-image"), "icon-name",
			G_BINDING_SYNC_CREATE,
			(GBindingTransformFunc) binding_lomo_state_to_gtk_stock, NULL
		},
		{
			"state",
			(GObject *) gel_ui_generic_get_typed(self, GTK_ACTIVATABLE, "play-pause-button"), "related-action",
			G_BINDING_SYNC_CREATE,
			(GBindingTransformFunc) binding_play_pause_lomo_state_to_action, NULL
		},
		{
			"volume",
			gtk_builder_get_object(builder, "volume-button"), "value",
			G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
			(GBindingTransformFunc) binding_volume_int_to_double_cb, binding_volume_double_to_int_cb
		},
	};

	for (guint i = 0; i < G_N_ELEMENTS(bindings); i++)
	{
		if ((bindings[i].src2dst == NULL) && (bindings[i].dst2src == NULL))
			g_object_bind_property(lomo, bindings[i].s_prop,
				bindings[i].dest, bindings[i].d_prop,
				bindings[i].flags);
		else
			g_object_bind_property_full(lomo, bindings[i].s_prop,
				bindings[i].dest, bindings[i].d_prop,
				bindings[i].flags,
				bindings[i].src2dst, bindings[i].dst2src,
				self, NULL);
	}

	g_object_set(priv->seek,  "lomo-player", lomo, NULL);
	g_object_set(priv->cover, "lomo-player", lomo, NULL);

	player_update_information(self);

	g_object_notify(G_OBJECT(self), "lomo-player");
}

void
eina_player_set_default_pixbuf(EinaPlayer *self, GdkPixbuf *pixbuf)
{
	g_return_if_fail(EINA_IS_PLAYER(self));
	EinaPlayerPrivate *priv = self->priv;

	if (pixbuf)
		g_return_if_fail(GDK_IS_PIXBUF(pixbuf));

	eina_cover_set_default_pixbuf(priv->cover, pixbuf);

	g_object_notify(G_OBJECT(self), "default-pixbuf");
}

/**
 * eina_player_get_cover_widget:
 * @self: An #EinaPlayer
 *
 * Returns the associated #EinaCover
 *
 * Returns: (transfer none): the #EinaCover
 */
EinaCover *
eina_player_get_cover_widget(EinaPlayer *self)
{
	g_return_val_if_fail(EINA_IS_PLAYER(self), NULL);
	return self->priv->cover;
}

/**
 * eina_player_get_plugins_area:
 * @self: An #EinaPlayer
 *
 * Gets the content area (a GtkHBox for now) avaliable for place widgets from plugins
 *
 * Returns: (transfer none): The content area
 */
GtkWidget *
eina_player_get_plugins_area(EinaPlayer *self)
{
	g_return_val_if_fail(EINA_IS_PLAYER(self), NULL);
	return gel_ui_generic_get_widget(GEL_UI_GENERIC(self), "plugins-content-area");
}

static void
player_update_information(EinaPlayer *self)
{
	g_return_if_fail(EINA_IS_PLAYER(self));
	EinaPlayerPrivate *priv = self->priv;

	LomoStream *stream = lomo_player_get_current_stream(priv->lomo);

	const gchar *line1 = NULL;
	const gchar *line2 = NULL;
	gchar *fallback = NULL;

	// Build text for labels
	if (stream)
	{
		const GValue *title_v  = lomo_stream_get_tag(stream, LOMO_TAG_TITLE);
		const GValue *artist_v = lomo_stream_get_tag(stream, LOMO_TAG_ARTIST);
		line1 = title_v  ? g_value_get_string(title_v)  : NULL;
		line2 = artist_v ? g_value_get_string(artist_v) : NULL;

		if (line1 == NULL)
		{
			gchar *uri_unescaped = g_uri_unescape_string(lomo_stream_get_uri(stream), NULL);
			fallback = g_path_get_basename(uri_unescaped);
			g_free(uri_unescaped);
		}
	}

	if (!line1)
		line1 = _("Eina music player");

	if (!line2)
		line2 = "\u200B";

	// Setup labels
	struct {
		gchar *widget_name;
		gchar *markup;
	} markups[2] =
	{
		{ "stream-title-label",  NULL },
		{ "stream-artist-label", NULL }
	};

	gchar *tmp = g_markup_escape_text(fallback ? fallback : line1, -1);
	markups[0].markup = g_strdup_printf("<span size=\"x-large\" weight=\"bold\">%s</span>", tmp);
	g_free(tmp);

	tmp = g_markup_escape_text(line2, -1);
	markups[1].markup = g_strdup_printf("<span size=\"x-large\" weight=\"normal\">%s</span>", tmp);
	g_free(tmp);

	gel_str_free_and_invalidate(fallback);

	for (guint i = 0; i < G_N_ELEMENTS(markups); i++)
	{
		g_object_set(gel_ui_generic_get_object(GEL_UI_GENERIC(self), markups[i].widget_name),
			"label", markups[i].markup,
			"ellipsize", stream ? PANGO_ELLIPSIZE_MIDDLE : PANGO_ELLIPSIZE_NONE,
			NULL);
		g_free(markups[i].markup);
	}

	// Window title
	GtkWindow *window = (GtkWindow *) gtk_widget_get_toplevel((GtkWidget *) self);
	if (window && (!GTK_IS_WINDOW(window) || !gtk_widget_is_toplevel((GtkWidget *) window)))
		window = NULL;

	if (window)
	{
		if (!stream)
			gtk_window_set_title(window, N_("Eina player"));
		else
		{
			gchar *title = gel_str_parser("{%a - }%t", (GelStrParserFunc) lomo_stream_string_parser_cb, stream);
			gtk_window_set_title(window, title);
			g_free(title);
		}
	}
}

// --
// Signals
// --
static void
lomo_all_tags_cb(LomoPlayer *lomo, LomoStream *stream, EinaPlayer *self)
{
	if (stream == lomo_player_get_current_stream(lomo))
		player_update_information(self);
}

static void
action_activated_cb(GtkAction *action, EinaPlayer *self)
{
	gboolean ret = FALSE;
	g_signal_emit(self, eina_player_signals[ACTION_ACTIVATED], 0, action, &ret);

	if (ret)
		return;

	const gchar *name = gtk_action_get_name(action);
	g_return_if_fail(name != NULL);

	LomoPlayer  *lomo = eina_player_get_lomo_player(self);
	g_return_if_fail(LOMO_IS_PLAYER(lomo));

	GError *error = NULL;

	gboolean is_play  = g_str_equal(name, "play-action");
	gboolean is_pause = g_str_equal(name, "pause-action");
	gboolean is_prev  = g_str_equal(name, "prev-action");
	gboolean is_next  = g_str_equal(name, "next-action");

	if (is_play || is_pause)
		lomo_player_set_state(lomo, is_play ? LOMO_STATE_PLAY : LOMO_STATE_PAUSE, &error);

	else if (is_prev || is_next)
		lomo_player_set_current(lomo, is_prev ? lomo_player_get_previous(lomo) : lomo_player_get_next(lomo), &error);

	else
	{
		g_warning(N_("Action %s was not handled"), name);
		return;
	}

	if (error)
	{
		g_warning(_("Error executing action '%s': %s"), name, error->message);
		g_error_free(error);
	}
}

/*
 * GBindingTransformFuncs for object bindings
 */
static gboolean
binding_volume_int_to_double_cb(GBinding *binding, const GValue *src, GValue *dst, gpointer data)
{
	g_value_set_double(dst, CLAMP(g_value_get_int(src) / (gdouble) 100, 0, 1));
	return TRUE;
}

static gboolean
binding_volume_double_to_int_cb(GBinding *binding, const GValue *src, GValue *dst, gpointer data)
{
	g_value_set_int(dst, (gint) CLAMP(g_value_get_double(src) * 100, 0, 100));
	return TRUE;
}

static gboolean
binding_lomo_state_to_gtk_stock(GBinding *binding, const GValue *src, GValue *dst, EinaPlayer *self)
{
	g_value_set_static_string(dst, (g_value_get_enum(src) == LOMO_STATE_PLAY) ?
		"media-playback-pause-symbolic" : "media-playback-start-symbolic");
	return TRUE;
}

static gboolean
binding_play_pause_lomo_current_to_sensitive(GBinding *binding, const GValue *src, GValue *dst, EinaPlayer *self)
{
	g_value_set_boolean(dst, g_value_get_int(src) != -1);
	return TRUE;
}

static gboolean
binding_play_pause_lomo_state_to_action(GBinding *binding, const GValue *src, GValue *dst, EinaPlayer *self)
{
	LomoState state = g_value_get_enum(src);
	GtkAction *action = NULL;

	if (state == LOMO_STATE_PLAY)
		action = gel_ui_generic_get_typed(self, GTK_ACTION, "pause-action");
	else
		action = gel_ui_generic_get_typed(self, GTK_ACTION, "play-action");

	g_value_set_object(dst, action);
	return TRUE;
}

