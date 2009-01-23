/*
 * plugins/adb/adb.c
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

#define GEL_DOMAIN "Eina::Plugin::Adb"
#include <eina/eina-plugin.h>
#include <plugins/adb/adb.h>
#include "register.h"

static void
adb_register_connect_lomo(Adb *self, LomoPlayer *lomo);
static void
adb_register_disconnect_lomo(Adb *self, LomoPlayer *lomo);
static void
app_plugin_init_cb(GelApp *app, GelPlugin *plugin, Adb *self);
static void
app_plugin_fini_cb(GelApp *app, GelPlugin *plugin, Adb *self);
static void
lomo_add_cb(LomoPlayer *lomo, LomoStream *stream, gint pos, gpointer data);
static void
lomo_del_cb(LomoPlayer *lomo, gint pos, gpointer data);
static void
lomo_clear_cb(LomoPlayer *lomo, gpointer data);

gboolean
adb_register_setup_0(Adb *self, gpointer data, GError **error)
{
	gchar *q[] = {
		"DROP TABLE IF EXISTS streams;",

		"CREATE TABLE IF NOT EXISTS streams ("
		"	sid INTEGER PRIMARY KEY AUTOINCREMENT,"
		"	uri VARCHAR(1024) UNIQUE NOT NULL,"
		"	timestamp TIMESTAMP NOT NULL,"
		"	played INTEGER DEFAULT 0,"
		"	last_play TIMESTAMP);",

		"DROP TABLE IF EXISTS playlist_history;",

		"CREATE TABLE IF NOT EXISTS playlist_history("
		"	timestamp TIMESTAMP NOT NULL,"
		"	uri VARCHAR(1024) NOT NULL);",

		NULL
	};
	return adb_exec_queryes(self, q, NULL, error);
}

void
adb_register_enable(Adb *self)
{
	gpointer callbacks[] = {
		adb_register_setup_0,
		NULL
		};

	GError *error = NULL;
	if (!adb_schema_upgrade(self, "register", callbacks, NULL, &error))
	{
		gel_error("Cannot enable register: %s", error->message);
		g_error_free(error);
		return;
	}
	char *q = "DELETE FROM playlist_history WHERE timestamp = 0;";
	sqlite3_exec(self->db, q, NULL, NULL, NULL);

	LomoPlayer *lomo = GEL_APP_GET_LOMO(self->app);
	if (lomo == NULL)
		g_signal_connect(self->app, "plugin-init", (GCallback) app_plugin_init_cb, self);
	else
		adb_register_connect_lomo(self, lomo);

}

void
adb_register_disable(Adb *self)
{
	LomoPlayer *lomo = GEL_APP_GET_LOMO(self->app);
	if (lomo != NULL)
		adb_register_disconnect_lomo(self, lomo);
}

// --
// Create table for registers
// --
void
adb_register_setup_db(Adb *self)
{
	gint   schema_version;
	gchar *schema_version_str = adb_variable_get(self, "schema-version");
	if (schema_version_str == NULL)
		schema_version = -1;
	else
	{
		schema_version = atoi((gchar *) schema_version_str);
		g_free(schema_version_str);
	}
	gel_warn("Got schema version: %d", schema_version);
}

// --
// Connect 'add' from lomo
// Disconnect 'plugin-init' form app
// Connect 'plugin-fini' from app
// --
static void
adb_register_connect_lomo(Adb *self, LomoPlayer *lomo)
{
	if (lomo == NULL) return;

	g_signal_handlers_disconnect_by_func(self->app, app_plugin_init_cb, self);
	g_signal_connect(self->app, "plugin-fini", (GCallback) app_plugin_fini_cb, self);
	g_signal_connect(lomo,      "add",         (GCallback) lomo_add_cb,        self);
	g_signal_connect(lomo,      "del",         (GCallback) lomo_del_cb,        self);
	g_signal_connect(lomo,      "clear",       (GCallback) lomo_clear_cb,      self);
}

// --
// Disconnect 'add' from lomo
// Connect 'plugin-init' from app
// Disconnect 'plugin-fini' from app
static void
adb_register_disconnect_lomo(Adb *self, LomoPlayer *lomo)
{
	if (lomo == NULL) return;

	g_signal_handlers_disconnect_by_func(lomo, lomo_add_cb, self);
	g_signal_handlers_disconnect_by_func(lomo, lomo_del_cb, self);
	g_signal_handlers_disconnect_by_func(lomo, lomo_clear_cb, self);
	g_signal_connect(self->app, "plugin-init", (GCallback)  app_plugin_init_cb, self);
	g_signal_handlers_disconnect_by_func(self->app, app_plugin_fini_cb, self);
}

// --
// Handle load/unload of lomo plugin
// --
static void
app_plugin_init_cb(GelApp *app, GelPlugin *plugin, Adb *self)
{
	if (!g_str_equal(plugin->name, "lomo"))
		return;
	adb_register_connect_lomo(self, GEL_APP_GET_LOMO(app));
}

static void
app_plugin_fini_cb(GelApp *app, GelPlugin *plugin, Adb *self)
{
	if (!g_str_equal(plugin->name, "lomo"))
		return;
	adb_register_disconnect_lomo(self, gel_app_shared_get(app, "lomo"));
}

// --
// Register each stream added
// --
static void
lomo_add_cb(LomoPlayer *lomo, LomoStream *stream, gint pos, gpointer data)
{
	Adb *self = (Adb *) data;
	// char *errmsg = NULL;

	gchar *uri = (gchar*) lomo_stream_get_tag(stream, LOMO_TAG_URI);
	gchar *q[2];
	q[0] = sqlite3_mprintf(
		"INSERT OR REPLACE INTO streams (sid,uri,timestamp) VALUES("
			"(SELECT sid FROM streams WHERE uri='%q'),"
			"'%q',"
			"DATETIME('NOW'));",
			uri,
			uri);
	q[1] = sqlite3_mprintf( "INSERT INTO playlist_history VALUES(0,'%q')", uri);
	q[2] = NULL;

	GError *error = NULL;
	if (!adb_exec_queryes(self, q, NULL, &error))
	{
		gel_error(error->message);
		g_error_free(error);
	}
	sqlite3_free(q[0]);
	sqlite3_free(q[1]);
}

static void
lomo_del_cb(LomoPlayer *lomo, gint pos, gpointer data)
{
	Adb *self = (Adb *) data;
	const LomoStream *stream = lomo_player_get_nth(lomo, pos);
	char *q = sqlite3_mprintf("DELETE FROM playlist_history WHERE uri='%q' AND timestamp=0;",
		lomo_stream_get_tag(stream, LOMO_TAG_URI));
	char *err = NULL;
	if (sqlite3_exec(self->db, q, NULL, NULL, &err) != SQLITE_OK)
	{
		gel_warn("Cannot update playlist_history: %s", err);
		sqlite3_free(err);
	}
}

static void
lomo_clear_cb(LomoPlayer *lomo, gpointer data)
{
	Adb *self = (Adb *) data;
	char *err = NULL;
	gchar *q = "update playlist_history set timestamp=DATETIME('NOW') where timestamp=0;";
	if (sqlite3_exec(self->db, q, NULL, NULL, &err) != SQLITE_OK)
	{
		gel_warn("Cannot update playlist_history: %s", err);
		sqlite3_free(err);
	}

}
