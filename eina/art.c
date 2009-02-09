#include "eina/art.h"
#include <eina/eina-plugin.h>
#include <config.h>

struct _Art {
	GList *backends; // updated by art_(add|remove)_backend
	GList *searches; // updated by art_search, art_cancel
};

struct _ArtBackend {
	Art     *art;
	ArtFunc  search, cancel;
	GList   *searches;
	gpointer data;
	gboolean enabled;
};

struct _ArtSearch {
	Art *art;
	LomoStream *stream;
	ArtFunc success, fail;
	gpointer result;
	gpointer data;
	GList *backend_link;
	gboolean running;
};

static void
art_forward_search(Art *art, ArtSearch *search);

void
art_run_search(Art *art, ArtSearch *search);
void
art_run_fail(Art *art, ArtSearch *search);

/*
// Run seach and cancel from hooks
static void
art_run_search(Art *art, ArtSeach *search);
static void
art_run_cancel(Art *art, ArtSeach *seach);
*/

void
art_backend_inject_search(ArtBackend *backend, ArtSearch *search)
{
	if (search->running)
	{
		g_warning("Search is running cannot be injected");
		return;
	}

	if (search->backend_link->data != backend)
	{
		g_warning("Search is not linked to this backend");
		return;
	}

	if (g_list_find(backend->searches, search))
	{
		g_warning("Search is already in this backend, this is not possible");
		return;
	}

	// Add to backend searches, search's link its already ok
	backend->searches = g_list_append(backend->searches, search);

	// Set as running
	search->running = TRUE;

	// Run now, no reason to delay this
	backend->search(backend->art, search, backend->data);
}

void
art_backend_extract_search(ArtBackend *backend, ArtSearch *search)
{
	if (search->running)
	{
		g_warning("Search is still running, you must cancel it first");
		return;
	}

	if ((search->backend_link->data != backend) || !g_list_find(backend->searches, search))
	{
		g_warning("Search has nothing to do with this backend");
		return;
	}

	backend->searches = g_list_remove(backend->searches, search);
}

Art*
art_new(void)
{
	Art *art = g_new0(Art, 1);
	return art;
}

void
art_destroy(Art *art)
{
	g_free(art);
}

// --
// At the beginning of the time we need to add some backends and do some real
// work
// --
ArtBackend *
art_add_backend(Art *art, ArtFunc search, ArtFunc cancel, gpointer data)
{
	ArtBackend *backend = g_new0(ArtBackend, 1);
	backend->search  = search;
	backend->cancel  = cancel;
	backend->data    = data;
	backend->enabled = TRUE;
	art->backends = g_list_append(art->backends, backend);
	return backend;
}

void
art_remove_backend(Art *art, ArtBackend *backend)
{
	// Disable
	backend->enabled = FALSE;
	
	// Forward all searches in this backend
	GList *iter = backend->searches;
	while (iter)
	{
		if (backend->cancel)
			backend->cancel(art,  (ArtSearch *) iter->data, backend->cancel);
		art_forward_search(art, (ArtSearch *) iter->data);

		iter = iter->next;
	}

	// Delete backend
	art->backends = g_list_remove(art->backends, backend);
	g_free(backend);
}

// --
// Once we have some real code we need to handle searches and cancelations from
// the app
// --
ArtSearch*
art_search(Art *art, LomoStream *stream, ArtFunc success, ArtFunc fail, gpointer data)
{
	ArtSearch *search = g_new0(ArtSearch, 1);
	search->stream  = stream;
	search->success = success;
	search->fail    = fail;
	search->data    = data;
	search->backend_link = art->backends;

	art->searches = g_list_append(art->searches, search);

	art_run_search(art, search);
	return search;
}

void
art_cancel(Art *art, ArtSearch *search)
{
	ArtBackend *backend = search->backend_link->data;

	// Just call cancel hook if exists and delete
	if (backend->cancel)
		backend->cancel(art, search, backend->data);

	backend->searches = g_list_remove(backend->searches, search);
	art->searches     = g_list_remove(art->searches, search);

	g_free(search);
}

// --
// Backends need to report Art object his success or failure, these two
// functions do the work
// --
void
art_report_success(Art *art, ArtSearch *search, gpointer result)
{
	search->result = result;
	if (search->success)
		search->success(art, search, search->data);

	// Forget search
	search->running = FALSE;

	ArtBackend *backend = (ArtBackend *) search->backend_link->data;
	backend->searches = g_list_remove(backend->searches, search);
	art->searches     = g_list_remove(art->searches, search);

	g_free(search);
}

void
art_report_failure(Art *art, ArtSearch *search)
{
	ArtBackend *backend = (ArtBackend *) search->backend_link->data;
	backend->searches = g_list_remove(backend->searches, search);
	art_forward_search(art, search);
}

// --
// More than one time searches can fail. This function moves failed searches
// over the backend list and runs them
// --
static void
art_forward_search(Art *art, ArtSearch *search)
{
	GList *iter = search->backend_link->next;
	
	// Find next enabled backend
	while (iter && !((ArtBackend *) iter->data)->enabled)
		iter = iter->next;

	// No more backends, fail
	if (iter == NULL)
	{
		if (search->fail)
			search->fail(art, search, search->data);
		art->searches = g_list_remove(art->searches, search);
		g_free(search);
	}
	else
	{
		ArtBackend *backend = (ArtBackend *) iter->data;
		search->running = TRUE;
		backend->searches = g_list_append(backend->searches, search);
		backend->search(art, search, backend->data);
	}
}

void
art_run_search(Art *art, ArtSearch *search)
{
	ArtBackend *backend = (ArtBackend *) search->backend_link->data;

	if (backend == NULL)
		art_run_fail(art, search);
	else
	{
		search->running   = TRUE;
		backend->searches = g_list_append(backend->searches, search);
		backend->search(art, search, backend->data);
	}
}


void
art_run_fail(Art *art, ArtSearch *search)
{
	search->fail(art, search, search->data);
	art->searches = g_list_remove(art->searches, search);
	g_free(search);
}

static gboolean
plugin_init(GelApp *app, GelPlugin *plugin, GError **error)
{
	plugin->data = art_new();
	gel_app_shared_set(app, "art", plugin->data);
	return TRUE;
}

static gboolean
plugin_fini(GelApp *app, GelPlugin *plugin, GError **error)
{
	art_destroy((Art *) plugin->data);
	gel_app_shared_unregister(app, "art");
	return TRUE;
}

G_MODULE_EXPORT EinaPlugin art_plugin = {
	EINA_PLUGIN_SERIAL,
	"art", PACKAGE_VERSION,
	N_("Art"), N_("New art framework"), NULL,
	EINA_PLUGIN_GENERIC_AUTHOR, EINA_PLUGIN_GENERIC_URL,
	plugin_init, plugin_fini,
	NULL, NULL
};

