/*
 * eina/preferences/eina-preferences-tab.h
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

#ifndef __EINA_PREFERENCES_TAB_H__
#define __EINA_PREFERENCES_TAB_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EINA_TYPE_PREFERENCES_TAB eina_preferences_tab_get_type()

#define EINA_PREFERENCES_TAB(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), EINA_TYPE_PREFERENCES_TAB, EinaPreferencesTab))

#define EINA_PREFERENCES_TAB_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), EINA_TYPE_PREFERENCES_TAB, EinaPreferencesTabClass))

#define EINA_IS_PREFERENCES_TAB(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EINA_TYPE_PREFERENCES_TAB))

#define EINA_IS_PREFERENCES_TAB_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), EINA_TYPE_PREFERENCES_TAB))

#define EINA_PREFERENCES_TAB_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), EINA_TYPE_PREFERENCES_TAB, EinaPreferencesTabClass))

typedef struct {
	GtkBox parent;
} EinaPreferencesTab;

typedef struct {
	GtkBoxClass parent_class;
	void (*changed) (const gchar *key, GValue *value);
} EinaPreferencesTabClass;

/**
 * EinaPreferencesTabEntry:
 * @settings_key: Key to bind from GSettings
 * @object_name: Object in the UI that represents the setting
 * @property: Property from @object_name that holds the value for @settings_key
 *
 * #EinaPreferencesTabEntry is the way to bind settings to UI. See
 * eina_preferences_tab_bind() and friends.
 */
typedef struct {
	gchar *settings_key;
	gchar *object_name;
	gchar *property;
} EinaPreferencesTabEntry;

GType eina_preferences_tab_get_type (void);

EinaPreferencesTab* eina_preferences_tab_new (void);

void       eina_preferences_tab_set_label_widget(EinaPreferencesTab *self, GtkWidget *label_widget);
void       eina_preferences_tab_set_label_image (EinaPreferencesTab *self, GtkImage *label_widget);
void       eina_preferences_tab_set_label_text  (EinaPreferencesTab *self, gchar *text);
void       eina_preferences_tab_set_widget      (EinaPreferencesTab *self, GtkWidget *widget);
void       eina_preferences_tab_set_ui_string   (EinaPreferencesTab *self, gchar *ui_string);
GtkWidget* eina_preferences_tab_get_widget      (EinaPreferencesTab *self, gchar *name);
GtkWidget* eina_preferences_tab_get_label_widget(EinaPreferencesTab *self);

void eina_preferences_tab_bind(EinaPreferencesTab *self,
	GSettings *settings, gchar *settings_key, gchar *object_name, gchar *property);
void eina_preferences_tab_bind_entries(EinaPreferencesTab *self,
	GSettings *settings, guint n_entries, EinaPreferencesTabEntry entries[]);
void eina_preferences_tab_bindv(EinaPreferencesTab *self, ...);

G_END_DECLS

#endif /* __EINA_PREFERENCES_TAB_H__ */
