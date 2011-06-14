/*
 * eina/mpris/eina-mpris-plugin.c
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

#include "eina-mpris-player.h"
#if HAVE_INDICATE
#include <libindicate/server.h>
#endif
#include <eina/ext/eina-extension.h>
#include <eina/lomo/eina-lomo-plugin.h>

#define EINA_TYPE_MPRIS_PLUGIN         (eina_mpris_plugin_get_type ())
#define EINA_MPRIS_PLUGIN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EINA_TYPE_MPRIS_PLUGIN, EinaMprisPlugin))
#define EINA_MPRIS_PLUGIN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k),     EINA_TYPE_MPRIS_PLUGIN, EinaMprisPlugin))
#define EINA_IS_MPRIS_PLUGIN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EINA_TYPE_MPRIS_PLUGIN))
#define EINA_IS_MPRIS_PLUGIN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k),    EINA_TYPE_MPRIS_PLUGIN))
#define EINA_MPRIS_PLUGIN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  EINA_TYPE_MPRIS_PLUGIN, EinaMprisPluginClass))

EINA_DEFINE_EXTENSION_HEADERS(EinaMprisPlugin, eina_mpris_plugin)

typedef struct {
	EinaMprisPlayer *emp;
	#if HAVE_INDICATE
	IndicateServer  *is;
	#endif
} EinaMprisData;

EINA_DEFINE_EXTENSION(EinaMprisPlugin, eina_mpris_plugin, EINA_TYPE_MPRIS_PLUGIN)

static gboolean
eina_mpris_plugin_activate(EinaActivatable *activatable, EinaApplication *app, GError **error)
{
	EinaMprisData *self = g_new(EinaMprisData, 1);

	self->emp = eina_mpris_player_new(app, PACKAGE);

	#if HAVE_INDICATE
	self->is = indicate_server_ref_default();
	indicate_server_set_type(self->is, "music."PACKAGE);

	gchar *uc_package = g_ascii_strup(PACKAGE, -1);
	gchar *env_package_name = g_strconcat(uc_package, "_DESKTOP_FILE", NULL);
	const gchar *env_desktop_file = g_getenv(env_package_name);
	if (env_desktop_file)
	{
		g_free(uc_package);
		g_free(env_package_name);
		g_warning("Set indicate desktop file to: '%s'", env_desktop_file);
		indicate_server_set_desktop_file(self->is, env_desktop_file);
	}
	else
	{
		gchar *tmp = g_strconcat(PACKAGE, ".desktop", NULL);
		gchar *desktop_file = g_build_filename(PACKAGE_PREFIX, "share", "applications", tmp, NULL);
		g_warning("Set indicate desktop file to: '%s'", desktop_file);
		indicate_server_set_desktop_file(self->is, desktop_file);
		g_free(tmp);
		g_free(desktop_file);
	}
	indicate_server_show(self->is);
	#endif

	eina_activatable_set_data(activatable, self);

	return TRUE;
}

static gboolean
eina_mpris_plugin_deactivate(EinaActivatable *activatable, EinaApplication *app, GError **error)
{
	EinaMprisData *self = eina_activatable_steal_data(activatable);
	gel_free_and_invalidate(self->emp, NULL, g_object_unref);
	#if HAVE_INDICATE
	gel_free_and_invalidate(self->is, NULL, g_object_unref);
	#endif
	g_free(self);

	return TRUE;
}
