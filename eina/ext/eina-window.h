/*
 * eina/ext/eina-window.h
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

#ifndef _EINA_WINDOW
#define _EINA_WINDOW

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EINA_TYPE_WINDOW eina_window_get_type()

#define EINA_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), EINA_TYPE_WINDOW, EinaWindow))

#define EINA_WINDOW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), EINA_TYPE_WINDOW, EinaWindowClass))

#define EINA_IS_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EINA_TYPE_WINDOW))

#define EINA_IS_WINDOW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), EINA_TYPE_WINDOW))

#define EINA_WINDOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), EINA_TYPE_WINDOW, EinaWindowClass))

typedef struct {
	GtkWindow parent;
} EinaWindow;

typedef struct {
	GtkWindowClass parent_class;
} EinaWindowClass;

typedef gboolean (*EinaWindowKeyBindHandler) (GdkEvent *ev, gpointer data);
typedef struct {
	guint keyval; // Use GDK_VoidSymbol to catch all
	EinaWindowKeyBindHandler handler;
} EinaWindowKeyBind;

GType eina_window_get_type (void);

EinaWindow* eina_window_new (void);

void
eina_window_set_persistant(EinaWindow *self, gboolean persistant);
gboolean
eina_window_get_persistant(EinaWindow *self);

gboolean
eina_window_key_bindings_get_enabled(EinaWindow *self);
void
eina_window_key_bindings_set_enabled(EinaWindow *self, gboolean enabled);

void
eina_window_add_widget(EinaWindow *self, GtkWidget *child, gboolean expand, gboolean fill, gint spacing);
void
eina_window_remove_widget(EinaWindow *self, GtkWidget *child);

GtkUIManager *
eina_window_get_ui_manager(EinaWindow *self);

void
eina_window_register_key_bindings(EinaWindow *self, guint n, EinaWindowKeyBind bind[], gpointer data);
void
eina_window_unregister_key_bindings(EinaWindow *self, guint n, EinaWindowKeyBind bind[], gpointer data);

G_END_DECLS

#endif /* _EINA_WINDOW */
