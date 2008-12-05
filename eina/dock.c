#define GEL_DOMAIN "Eina::Dock"

#include <string.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <gel/gel.h>
#include <eina/dock.h>
#include <eina/player.h>
#include <eina/settings.h>

struct _EinaDock {
	EinaBase     parent;

	EinaConf    *conf;

	GtkWidget   *widget;
	GtkNotebook *dock;
	GHashTable  *dock_items;
	GList       *dock_idx;

	gint         w, h;
};

static void
page_reorder_cb(GtkNotebook *w, GtkWidget *widget, guint n, EinaDock *self);

static void
expander_activate_cb(GtkExpander *w, EinaDock *self);

static gboolean
player_main_window_configure_event_cb(GtkWidget *w, GdkEventConfigure *event, EinaDock *self);

gboolean
dock_init(GelHub *hub, gint *argc, gchar ***argv)
{
	EinaDock *self;

	self = g_new0(EinaDock, 1);
	if (!eina_base_init(EINA_BASE(self), hub, "dock", EINA_BASE_GTK_UI))
	{
		gel_error("Cannot create component");
		return FALSE;
	}

	// if ((self->conf = EINA_BASE_GET_SETTINGS(self)) == NULL)
	if ((self->conf = eina_base_require(EINA_BASE(self), "settings")) == NULL)
	{
		gel_error("Cannot access settings");
		eina_base_fini(EINA_BASE(self));
		return FALSE;
	}

	// if (EINA_BASE_GET_PLAYER(self) == NULL)
	if (!eina_base_require(EINA_BASE(self), "player"))
	{
		gel_error("Player is not loaded, dock cannot be initialized");
		gel_hub_unload(HUB(self), "settings");
		eina_base_fini(EINA_BASE(self));
		return FALSE;
	}

	self->widget = W(self, "dock-expander");
	self->dock   = W_TYPED(self, GTK_NOTEBOOK, "dock-notebook");

	// Delete old keys
	eina_conf_delete_key(self->conf, "/ui/size_w");
	eina_conf_delete_key(self->conf, "/ui/size_h");

	// Configure the dock route table
	const gchar *order;
	gchar **split;

	order = eina_conf_get_str(self->conf, "/dock/order", "playlist");
	gel_list_deep_free(self->dock_idx, g_free);
	split = g_strsplit(order, ",", 0);
	self->dock_idx = gel_strv_to_list(split, FALSE);
	g_free(split); // NOT g_freestrv, look the FALSE in the prev. line

	// Handle tab-reorder
	gtk_widget_realize(GTK_WIDGET(self->dock));
	g_signal_connect(self->dock, "page-reordered",
		G_CALLBACK(page_reorder_cb), self);

	// Remove preexistent tabs
	gint i;
	for (i = 0; i < gtk_notebook_get_n_pages(self->dock); i++)
		gtk_notebook_remove_page(self->dock, i);
	self->dock_items = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
	gtk_notebook_set_show_tabs(self->dock, FALSE);

	// Transfer widget to player
	GtkWidget *parent = self->widget->parent;
	g_object_ref(self->widget);
	gtk_widget_show(self->widget);
	gtk_container_remove(GTK_CONTAINER(parent), self->widget);
	eina_player_add_widget(EINA_BASE_GET_PLAYER(self), self->widget);
	gtk_widget_destroy(parent);

	g_signal_connect(eina_player_get_main_window(EINA_BASE_GET_PLAYER(self)), "configure-event",
		(GCallback) player_main_window_configure_event_cb, self);

	g_signal_connect(self->widget, "activate", (GCallback) expander_activate_cb, self);

	return TRUE;
}

G_MODULE_EXPORT gboolean dock_exit
(gpointer data)
{
	EinaDock *self = EINA_DOCK(data);

	gel_hub_unload(HUB(self), "player");
	gel_hub_unload(HUB(self), "settings");

	eina_conf_set_int(self->conf, "/ui/main-window/width",  self->w);
	eina_conf_set_int(self->conf, "/ui/main-window/height", self->h);

	eina_base_fini(EINA_BASE(self));
	return TRUE;
}

gboolean
eina_dock_add_widget(EinaDock *self, gchar *id, GtkWidget *label, GtkWidget *dock_widget)
{
	gint pos = g_list_position(self->dock_idx,
		g_list_find_custom(self->dock_idx, id, (GCompareFunc) strcmp));

	if (!self->dock || (g_hash_table_lookup(self->dock_items, id) != NULL))
	{
		return FALSE;
	}

	g_hash_table_insert(self->dock_items, g_strdup(id), (gpointer) dock_widget);
	if (gtk_notebook_append_page(self->dock, dock_widget, label) == -1)
	{
		g_hash_table_remove(self->dock_items, id);
		gel_error("Cannot add widget to dock");
		return FALSE;
	}

	gel_info("Added dock '%s'", id);
	gtk_notebook_set_tab_reorderable(self->dock, dock_widget, TRUE);
	if (pos > -1)
	{
		gtk_notebook_reorder_child(self->dock, dock_widget, pos);
		if (pos <= gtk_notebook_get_current_page(self->dock))
			gtk_notebook_set_current_page(self->dock, pos);
	}

	if (gtk_notebook_get_n_pages(self->dock) > 1)
		gtk_notebook_set_show_tabs(self->dock, TRUE);
/*
	if (!GTK_WIDGET_VISIBLE(W(self->player, "dock-expander")))
		gtk_widget_show(W(self->player, "dock-expander"));
*/
	return TRUE;
}

gboolean
eina_dock_remove_widget(EinaDock *self, gchar *id)
{
	GtkWidget *dock_item;

	if (!self->dock || ((dock_item = g_hash_table_lookup(self->dock_items, id)) == NULL))
	{
		return FALSE;
	}

	gtk_notebook_remove_page(GTK_NOTEBOOK(self->dock), gtk_notebook_page_num(GTK_NOTEBOOK(self->dock), dock_item));

	switch (gtk_notebook_get_n_pages(self->dock) <= 1)
	{
	case 0:
		gtk_widget_hide(self->widget);
		break;
	case 1:
		gtk_notebook_set_show_tabs(self->dock, FALSE);
		break;
	default:
		break;
	}

	return g_hash_table_remove(self->dock_items, id);
}

gboolean
eina_dock_switch_widget(EinaDock *self, gchar *id)
{
	gint page_num;
	GtkWidget *dock_item;

	dock_item = g_hash_table_lookup(self->dock_items, (gpointer) id);
	if (dock_item == NULL)
	{
		gel_error("Cannot find dock widget '%s'", id);
		return FALSE;
	}

	page_num = gtk_notebook_page_num(GTK_NOTEBOOK(self->dock), dock_item);
	if (page_num == -1)
	{
		gel_error("Dock item %s is not in dock", id);
		return FALSE;
	}
	gtk_notebook_set_current_page(GTK_NOTEBOOK(self->dock), page_num);
	return TRUE;
}

static void
page_reorder_cb_aux (gpointer k, gpointer v, gpointer data)
{
	GList *list       = (GList *) data;
	GList *p;

	p = g_list_find(list, v);
	if (p == NULL)
		return;
	
	p->data = k;
}

static void
page_reorder_cb(GtkNotebook *w, GtkWidget *widget, guint n, EinaDock *self)
{
	gint n_tabs, i;
	GList *dock_items = NULL;
	GString *output = g_string_new(NULL);

	n_tabs = gtk_notebook_get_n_pages(w);
	for (i = (n_tabs - 1); i >= 0; i--)
	{
		dock_items = g_list_prepend(dock_items, gtk_notebook_get_nth_page(w, i));
	}

	if (self->dock_items != NULL)
		g_hash_table_foreach(self->dock_items, page_reorder_cb_aux, dock_items);
	for (i = 0; i < n_tabs; i++)
	{
		output = g_string_append(output, (gchar *) g_list_nth_data(dock_items, i));
		if ((i+1) < n_tabs)
			output = g_string_append_c(output, ',');
	}

	if (self->conf == NULL)
	{
		gel_error("Settings are not available, cannot save dock order");
	}
	else
	{
		eina_conf_set_str(self->conf, "/dock/order", output->str);
	}
	g_string_free(output, TRUE);
}

static void
expander_activate_cb(GtkExpander *w, EinaDock *self)
{
	gboolean expanded = gtk_expander_get_expanded(w);
	GtkWindow *win = eina_player_get_main_window(EINA_BASE_GET_PLAYER(self));

	if (expanded)
	{
		g_signal_handlers_disconnect_by_func(win, player_main_window_configure_event_cb, self);
		gtk_window_set_resizable(win, FALSE);
		gtk_window_resize(win, 1, 1);
	}
	else
	{
		g_signal_connect(win, "configure-event", (GCallback) player_main_window_configure_event_cb, self);
		gtk_window_set_resizable(win, TRUE);
		gtk_window_resize(win, self->w, self->h);
	}
}

static gboolean
player_main_window_configure_event_cb(GtkWidget *w, GdkEventConfigure *event, EinaDock *self)
{
	if (event->type != GDK_CONFIGURE)
		return FALSE;

	self->h = event->height;
	self->w = event->width;

	return FALSE; // Propagate event
}

G_MODULE_EXPORT GelHubSlave dock_connector = {
	"dock",
	&dock_init,
	&dock_exit
};

