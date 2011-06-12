/*
 * eina/player/eina-seek.c
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

#include "eina-seek.h"
#include <lomo/lomo-util.h>
#include <gel/gel.h>

G_DEFINE_TYPE (EinaSeek, eina_seek, GEL_UI_TYPE_SCALE)

enum {
	EINA_SEEK_TIME_CURRENT,
	EINA_SEEK_TIME_REMAINING,
	EINA_SEEK_TIME_TOTAL,
	EINA_SEEK_N_TIMES
};

enum {
	EINA_SEEK_PROPERTY_LOMO_PLAYER = 1,
	EINA_SEEK_PROPERTY_TIME_CURRENT_LABEL,
	EINA_SEEK_PROPERTY_TIME_REMAINING_LABEL,
	EINA_SEEK_PROPERTY_TIME_TOTAL_LABEL,
};

struct _EinaSeekPrivate {
	LomoPlayer *lomo;
	guint       updater_id, real_id;
	gint64      pos;
	gboolean    total_is_desync;

	gboolean disposed;

	GtkLabel *time_labels[EINA_SEEK_N_TIMES];
	gchar    *time_fmts[EINA_SEEK_N_TIMES];
	gchar    *time_def_val[EINA_SEEK_N_TIMES];
};

// --
// Internal funcions
// --
static void
eina_seek_set_generic_label(EinaSeek *self, gint id, GtkLabel *label);
static void
seek_update_values(EinaSeek *self, gint64 current_time, gint64 total_time, gboolean temp);
static void
seek_sync_values(EinaSeek *self);
static void
seek_updater_start(EinaSeek *self);
static void
seek_updater_stop(EinaSeek *self);

// --
// UI callbacks
// --
static void
value_changed_cb (GtkWidget *w, EinaSeek *self);
static gboolean
button_press_event_cb (GtkWidget *w, GdkEventButton *ev, EinaSeek *self);
static gboolean
button_release_event_cb (GtkWidget *w, GdkEventButton *ev, EinaSeek *self);
static gboolean
timeout_cb(EinaSeek *self);

// --
// Lomo callbacks
// --
static void
lomo_change_cb(LomoPlayer *lomo, gint from, gint to, EinaSeek *self);
static void
lomo_state_change_cb(LomoPlayer *lomo, EinaSeek *self);
static void
lomo_clear_cb(LomoPlayer *lomo, EinaSeek *self);

// --
// Get/Set properties
// --
static void
eina_seek_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	EinaSeek *self = EINA_SEEK(object);
	switch (property_id)
	{
	case EINA_SEEK_PROPERTY_LOMO_PLAYER:
		g_value_set_object(value, (gpointer) eina_seek_get_lomo_player(self));
		break;

	case EINA_SEEK_PROPERTY_TIME_CURRENT_LABEL:
		g_value_set_object(value, (gpointer) eina_seek_get_current_label(self));
		break;

	case EINA_SEEK_PROPERTY_TIME_REMAINING_LABEL:
		g_value_set_object(value, (gpointer) eina_seek_get_remaining_label(self));
		break;

	case EINA_SEEK_PROPERTY_TIME_TOTAL_LABEL:
		g_value_set_object(value, (gpointer) eina_seek_get_total_label(self));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
eina_seek_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	EinaSeek *self = EINA_SEEK(object);

	switch (property_id)
	{
	case EINA_SEEK_PROPERTY_LOMO_PLAYER:
		eina_seek_set_lomo_player(self, LOMO_PLAYER(g_value_get_object(value)));
		break;

	case EINA_SEEK_PROPERTY_TIME_CURRENT_LABEL:
		eina_seek_set_current_label(self, GTK_LABEL(g_value_get_object(value)));
		break;

	case EINA_SEEK_PROPERTY_TIME_REMAINING_LABEL:
		eina_seek_set_remaining_label(self, GTK_LABEL(g_value_get_object(value)));
		break;

	case EINA_SEEK_PROPERTY_TIME_TOTAL_LABEL:
		eina_seek_set_total_label(self, GTK_LABEL(g_value_get_object(value)));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

// --
// Dispose and finalize hooks
// --
static void
eina_seek_dispose (GObject *object)
{
	EinaSeek *self = EINA_SEEK(object);
	EinaSeekPrivate *priv = self->priv;
	if (!priv->disposed)
	{

		seek_updater_stop(EINA_SEEK(object));
	
		eina_seek_set_lomo_player(self, NULL);

		gint i;
		for (i = 0; i < EINA_SEEK_N_TIMES; i++)
			eina_seek_set_generic_label(self, i, NULL);

		priv->disposed = TRUE;
	}

	if (G_OBJECT_CLASS (eina_seek_parent_class)->dispose)
		G_OBJECT_CLASS (eina_seek_parent_class)->dispose (object);
}

// --
// Class init, init and new
// --
static void
eina_seek_class_init (EinaSeekClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (EinaSeekPrivate));

	object_class->get_property = eina_seek_get_property;
	object_class->set_property = eina_seek_set_property;
	object_class->dispose = eina_seek_dispose;

	g_object_class_install_property(object_class, EINA_SEEK_PROPERTY_LOMO_PLAYER,
		g_param_spec_object("lomo-player", "Lomo player", "LomoPlayer object to control/watch",
		LOMO_TYPE_PLAYER, G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT
		));

	g_object_class_install_property(object_class, EINA_SEEK_PROPERTY_TIME_CURRENT_LABEL,
		g_param_spec_object("current-label", "Current label", "GtkLabel widget to show current time",
		GTK_TYPE_LABEL, G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT
		));

	g_object_class_install_property(object_class, EINA_SEEK_PROPERTY_TIME_REMAINING_LABEL,
		g_param_spec_object("remaining-label", "Remaining label", "GtkLabel widget to show remaining time",
		GTK_TYPE_LABEL, G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT
		));

	g_object_class_install_property(object_class, EINA_SEEK_PROPERTY_TIME_TOTAL_LABEL,
		g_param_spec_object("total-label", "Total label", "GtkLabel widget to show total time",
		GTK_TYPE_LABEL, G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT
		));
}

static void
eina_seek_init (EinaSeek *self)
{
	EinaSeekPrivate *priv = self->priv = (G_TYPE_INSTANCE_GET_PRIVATE ((self), EINA_TYPE_SEEK, EinaSeekPrivate));

	gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_HORIZONTAL);

	priv->total_is_desync = TRUE;
	priv->lomo   = NULL;
	for (guint i = 0; i < EINA_SEEK_N_TIMES; i++)
	{
		priv->time_labels[i] = NULL;
		priv->time_fmts[i]   = NULL;
	}
    priv->real_id = -1;

}

EinaSeek*
eina_seek_new (void)
{
	EinaSeek *self = g_object_new (EINA_TYPE_SEEK, NULL);
	
	gtk_scale_set_draw_value(GTK_SCALE(self), FALSE);

	gtk_range_set_range(GTK_RANGE(self), 0, 1000);
	// Removed in gtk3, not sure if removing this creates any regression
	// gtk_range_set_update_policy(GTK_RANGE(self), GTK_UPDATE_CONTINUOUS);
	
	gtk_widget_set_sensitive(GTK_WIDGET(self), FALSE);

	g_signal_connect(self, "value-changed",        G_CALLBACK(value_changed_cb), self);
	g_signal_connect(self, "button-press-event",   G_CALLBACK(button_press_event_cb), self);
	g_signal_connect(self, "button-release-event", G_CALLBACK(button_release_event_cb), self);

	return self;
}

// --
// Getters and setters
// --
void
eina_seek_set_lomo_player(EinaSeek *self, LomoPlayer *lomo)
{
	g_return_if_fail(EINA_IS_SEEK(self));
	if (lomo != NULL)
		g_return_if_fail(LOMO_IS_PLAYER(lomo));

	EinaSeekPrivate *priv = self->priv;

	if (priv->lomo)
	{
		g_signal_handlers_disconnect_by_func(priv->lomo, seek_updater_start, self);
		g_signal_handlers_disconnect_by_func(priv->lomo, seek_updater_stop, self);
		g_signal_handlers_disconnect_by_func(priv->lomo, lomo_state_change_cb, self);
		g_signal_handlers_disconnect_by_func(priv->lomo, lomo_change_cb, self);
		g_signal_handlers_disconnect_by_func(priv->lomo, lomo_clear_cb, self);
		g_object_unref(priv->lomo);
	}

	if (lomo == NULL)
		return;

	priv->lomo = lomo;
	g_object_ref(lomo);

	// Connect UI signals
	g_signal_connect_swapped(lomo, "play",  G_CALLBACK(seek_updater_start), self);
	g_signal_connect_swapped(lomo, "pause", G_CALLBACK(seek_updater_stop),  self);
	g_signal_connect_swapped(lomo, "stop",  G_CALLBACK(seek_updater_stop),  self);

	// Connect lomo signals
	g_signal_connect(lomo, "play",   G_CALLBACK(lomo_state_change_cb), self);
	g_signal_connect(lomo, "pause",  G_CALLBACK(lomo_state_change_cb), self);
	g_signal_connect(lomo, "stop",   G_CALLBACK(lomo_state_change_cb), self);
	g_signal_connect(lomo, "change", G_CALLBACK(lomo_change_cb), self);
	g_signal_connect(lomo, "clear",  G_CALLBACK(lomo_clear_cb),  self);

	priv->total_is_desync = TRUE;
	seek_update_values(self, lomo_player_tell_time(priv->lomo), lomo_player_length_time(priv->lomo), FALSE);
	if (lomo_player_get_current(priv->lomo) >= 0)
		gtk_widget_set_sensitive(GTK_WIDGET(self), TRUE);
	else
		gtk_widget_set_sensitive(GTK_WIDGET(self), FALSE);
}

LomoPlayer *
eina_seek_get_lomo_player(EinaSeek *self)
{
	g_return_val_if_fail(EINA_IS_SEEK(self), NULL);
	return self->priv->lomo;
}

static void
eina_seek_set_generic_label(EinaSeek *self, gint id, GtkLabel *label)
{
	g_return_if_fail(EINA_IS_SEEK(self));
	g_return_if_fail(id < EINA_SEEK_N_TIMES);
	if (label != NULL)
		g_return_if_fail(GTK_IS_LABEL(label));

	EinaSeekPrivate *priv = self->priv;

	priv->time_labels[id] = NULL;
	gel_free_and_invalidate(priv->time_labels[id],  NULL, g_object_unref);
	gel_free_and_invalidate(priv->time_fmts[id],    NULL, g_free);
	gel_free_and_invalidate(priv->time_def_val[id], NULL, g_free);

	if (label == NULL)
		return;

	g_object_ref(label);
	priv->time_labels[id] = label;
	priv->time_fmts[id]   = g_strdup(gtk_label_get_label(label));

	gchar *d = priv->time_def_val[id] = g_strdup_printf(priv->time_fmts[id], 0, 0);
	for (guint i = 0; d[i] != '\0'; i++)
		if (d[i] == '0')
			d[i] = '-';
	gtk_label_set_label(label, d);
}

void
eina_seek_set_current_label(EinaSeek *self, GtkLabel *label)
{
	eina_seek_set_generic_label(self, EINA_SEEK_TIME_CURRENT, label);
}

void
eina_seek_set_remaining_label(EinaSeek *self, GtkLabel *label)
{
	eina_seek_set_generic_label(self, EINA_SEEK_TIME_REMAINING, label);
}

void
eina_seek_set_total_label(EinaSeek *self, GtkLabel *label)
{
	eina_seek_set_generic_label(self, EINA_SEEK_TIME_TOTAL, label);
}

LomoPlayer*
eina_seek_get_lomo(EinaSeek *self)
{
	g_return_val_if_fail(EINA_IS_SEEK(self), NULL);
	return self->priv->lomo;
}

GtkLabel*
eina_seek_get_current_label(EinaSeek *self)
{
	g_return_val_if_fail(EINA_IS_SEEK(self), NULL);
	return self->priv->time_labels[EINA_SEEK_TIME_CURRENT];
}

GtkLabel*
eina_seek_get_remaining_label(EinaSeek *self)
{
	g_return_val_if_fail(EINA_IS_SEEK(self), NULL);
	return self->priv->time_labels[EINA_SEEK_TIME_REMAINING];
}

GtkLabel*
eina_seek_get_total_label(EinaSeek *self)
{
	g_return_val_if_fail(EINA_IS_SEEK(self), NULL);
	return self->priv->time_labels[EINA_SEEK_TIME_TOTAL];
}

//--
// Private functions
//--

// Control updater timeout functions
static void
seek_updater_start(EinaSeek *self)
{
	EinaSeekPrivate *priv = self->priv;
	if (priv->updater_id > 0)
		g_source_remove(priv->updater_id);
	priv->updater_id = g_timeout_add(400, (GSourceFunc) timeout_cb, (gpointer) self);
}

static void
seek_updater_stop(EinaSeek *self)
{
	EinaSeekPrivate *priv = self->priv;
	if (priv->updater_id > 0)
	{
		g_source_remove(priv->updater_id);
		priv->updater_id = 0;
	}
}

static gboolean
seek_real_seek(EinaSeek *self)
{
	EinaSeekPrivate *priv = self->priv;
	g_return_val_if_fail(LOMO_IS_PLAYER(priv->lomo), FALSE);

	LomoState state = lomo_player_get_state(priv->lomo);
	if ((state == LOMO_STATE_STOP) || (state == LOMO_STATE_INVALID))
		lomo_player_set_state(priv->lomo, LOMO_STATE_PLAY, NULL);

	lomo_player_seek_time(priv->lomo, priv->pos);
	priv->pos = -1;

	return FALSE;
}

static gchar*
seek_fmt_time(EinaSeek *self, gint id, gint64 time, gboolean tempstr)
{
	g_return_val_if_fail(EINA_IS_SEEK(self), NULL);
	EinaSeekPrivate *priv = self->priv;

	if (time < 0)
		return NULL;

	gint secs = LOMO_NANOSECS_TO_SECS(time);

	gchar *ret = NULL;
	if (tempstr)
	{
		gchar *tmp = g_strdup_printf(priv->time_fmts[id], secs / 60, secs % 60);
		ret = g_strdup_printf("<i>%s</i>", tmp);
		g_free(tmp);
	}
	else
		ret = g_strdup_printf(priv->time_fmts[id], secs / 60, secs % 60);

	return ret;
}

static void
seek_update_values(EinaSeek *self, gint64 current_time, gint64 total_time, gboolean temp)
{
	EinaSeekPrivate *priv = self->priv;
	gchar *current, *remaining, *total;

	// Sync total only if not synced
	if (priv->total_is_desync && (priv->time_labels[EINA_SEEK_TIME_TOTAL] != NULL))
	{
		if (total_time >= 0)
		{
			total = seek_fmt_time(self, EINA_SEEK_TIME_TOTAL, total_time, FALSE);
			gtk_label_set_markup(priv->time_labels[EINA_SEEK_TIME_TOTAL], total);
			g_free(total);
			priv->total_is_desync = FALSE;
		}
		else
			gtk_label_set_markup(priv->time_labels[EINA_SEEK_TIME_TOTAL], priv->time_def_val[EINA_SEEK_TIME_TOTAL]);
	}
	// If total is -1 even if there is no widget to show it, self must be
	// insensitive and progress reset
	/*
	if (total_time < 0)
	{
		gtk_widget_set_sensitive(GTK_WIDGET(self), FALSE);
		gtk_range_set_value(GTK_RANGE(self), (gdouble) 0);
	}
	*/

	// Remaining
	if (priv->time_labels[EINA_SEEK_TIME_REMAINING])
	{
		if ((total_time >= 0) && (current_time >= 0) && (total_time >= current_time))
			remaining =  seek_fmt_time(self, EINA_SEEK_TIME_REMAINING, total_time - current_time, temp);
		else
			remaining = NULL;

		gtk_label_set_markup(priv->time_labels[EINA_SEEK_TIME_REMAINING],
			remaining ? remaining : priv->time_def_val[EINA_SEEK_TIME_REMAINING]);
		gel_free_and_invalidate(remaining, NULL, g_free);
	}

	// Current
	if (priv->time_labels[EINA_SEEK_TIME_CURRENT])
	{
		if (current_time >= 0)
			current = seek_fmt_time(self, EINA_SEEK_TIME_CURRENT, current_time, temp);
		else
			current = NULL;
		gtk_label_set_markup(priv->time_labels[EINA_SEEK_TIME_CURRENT],
			current ? current : priv->time_def_val[EINA_SEEK_TIME_CURRENT]);
		gel_free_and_invalidate(current, NULL, g_free);
   }
}

static void
seek_sync_values(EinaSeek *self)
{
	EinaSeekPrivate *priv = self->priv;
	if (priv->lomo == NULL)
		return;
	priv->total_is_desync = TRUE;
	seek_update_values(self, lomo_player_tell_time(priv->lomo), lomo_player_length_time(priv->lomo), FALSE);
}

// --
// UI Callbacks
// --
static void
value_changed_cb(GtkWidget *w, EinaSeek *self)
{
	EinaSeekPrivate *priv = self->priv;

	gint64  total;
	gint64  pseudo_pos;
	gdouble val;

	/* 
	 * Stop the timeout function if any 
	 * XXX: Figure out a better way to detect this
	 */
	if ((priv->pos != -1) && priv->real_id && (priv->real_id > 0))
		g_source_remove(priv->real_id);

	total = lomo_player_length_time(priv->lomo);
	val = gtk_range_get_value(GTK_RANGE(self));
	pseudo_pos = total * (val/1000);

	seek_update_values(self, pseudo_pos, total, TRUE);

	/* Create a timeout function */
	priv->pos = pseudo_pos;
	priv->real_id = g_timeout_add(100, (GSourceFunc) seek_real_seek, self);
}

static gboolean
button_press_event_cb(GtkWidget *w, GdkEventButton *ev, EinaSeek *self)
{
	seek_updater_stop(self);
	return FALSE;
}

static gboolean
button_release_event_cb(GtkWidget *w, GdkEventButton *ev, EinaSeek *self)
{
	seek_updater_start(self);
	return FALSE;
}

static gboolean
timeout_cb(EinaSeek *self)
{
	EinaSeekPrivate *priv = self->priv;
	gint64 len = 0, pos = 0;
	gdouble percent;
	
	len = lomo_player_length_time(priv->lomo);
	if ((lomo_player_get_state(priv->lomo) == LOMO_STATE_STOP) || len == 0 ) {
		percent = 0;
	} else {
		pos = lomo_player_tell_time(priv->lomo);
		percent = (gdouble)((pos * 1000) / len);
	}

    g_signal_handlers_block_by_func(
		self,
		value_changed_cb,
		self);
	gtk_range_set_value(GTK_RANGE(self), percent);
	g_signal_handlers_unblock_by_func(
		self,
		value_changed_cb,
		self);

	seek_update_values(self, pos, len, FALSE);
	return TRUE;
}

// --
// Lomo callbacks
// --
static void
lomo_change_cb(LomoPlayer *lomo, gint from, gint to, EinaSeek *self)
{
	EinaSeekPrivate *priv = self->priv;

	if (from == to)
		return;

	if ((from == -1) && (to >= 0))
	{
		gtk_widget_set_sensitive(GTK_WIDGET(self), TRUE);
	}
	if ((from >= 0) && (to == -1))
	{
		gtk_widget_set_sensitive(GTK_WIDGET(self), FALSE);
	}
	priv->total_is_desync = TRUE;

	gtk_range_set_value(GTK_RANGE(self), (gdouble) 0);
}

static void
lomo_clear_cb(LomoPlayer *lomo, EinaSeek *self)
{
	struct _EinaSeekPrivate *priv = self->priv;
	priv->total_is_desync = TRUE;
	gtk_widget_set_sensitive(GTK_WIDGET(self), FALSE);
	gtk_range_set_value(GTK_RANGE(self), (gdouble) 0);
	seek_update_values(self, -1, -1, FALSE);
}

static void
lomo_state_change_cb(LomoPlayer *lomo, EinaSeek *self)
{
	seek_sync_values(self);
}
