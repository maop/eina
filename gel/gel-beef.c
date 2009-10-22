#include <gel/gel-beef.h>
#include <glib/gprintf.h>
#include <string.h>

struct _GelBeefOp {
	GCancellable *cancellable;
	GQueue *queue;

	GelBeefSuccessFunc success_cb;
	GelBeefErrorFunc   error_cb;

	GNode *result;
	GFile *root;

	const gchar *attributes;

	gpointer data;

};

static void
_gel_beef_run_queue(GelBeefOp *self);
static void
_gel_beef_run_success(GelBeefOp *self);
static void
_gel_beef_run_error(GelBeefOp *self, GFile *source, GError *error);

// --
// Querying info
// --
static void
query_info_cb(GFile *source, GAsyncResult *res, GelBeefOp *self);

// --
// Enumerating children
// --
static void
enumerate_children_cb(GFile *source, GAsyncResult *res, GelBeefOp *self);
static void
enumerator_next_cb(GFileEnumerator *e, GAsyncResult *res, GelBeefOp *self);
#if 0
enumerator_close_cb(GFileEnumerator *e, GAsyncResult *res, GelBeefOp *self);
#endif

// --
// Comparing nodes
// --
static void
sort_children(GNode *parent);
static gint
compare_node_cb(GNode *a, GNode *b);

GelBeefOp *
gel_beef_walk(GFile *file, const gchar *attributes, gboolean recurse, 
  GelBeefSuccessFunc success_cb, GelBeefErrorFunc error_cb, gpointer data)
{
	GelBeefOp *self = g_new0(GelBeefOp, 1);
	self->cancellable = g_cancellable_new();
	self->queue = g_queue_new();
	self->attributes = attributes;
	self->success_cb  = success_cb;
	self->error_cb    = error_cb;

	self->result = g_node_new(file);
	self->root   = file;

	self->data = data;

	// Insert into our tree-queue
	g_object_set_data((GObject *) file, "gnode",  self->result);
	g_queue_push_tail(self->queue, file);

	_gel_beef_run_queue(self);

	return self;
}

static gboolean
disassociate_node_data(GNode *node, gpointer data)
{
	GFile     *file      = g_object_get_data((GObject *) node->data, "gfile");
	GFileInfo *file_info = g_object_get_data((GObject *) node->data, "gfileinfo");
	if (file)
		g_object_unref(file);
	if (file_info)
		g_object_unref(file_info);
	return FALSE;
}

void
gel_beef_close(GelBeefOp *self)
{
	if (self->cancellable)
	{
		g_cancellable_cancel(self->cancellable);
		g_object_unref(self->cancellable);
		self->cancellable = NULL;
	}
	if (self->queue)
	{
		g_queue_free(self->queue);
		self->queue = NULL;
	}
	if (self->result)
	{
		g_node_traverse(self->result, G_IN_ORDER, G_TRAVERSE_ALL, -1, disassociate_node_data, NULL);
		g_node_destroy(self->result);
		self->result = NULL;
	}
	g_free(self);
}

static void
_gel_beef_run_queue(GelBeefOp *self)
{
	if (g_queue_is_empty(self->queue))
	{
		_gel_beef_run_success(self);
		return;
	}
	g_cancellable_reset(self->cancellable);

	GFile *file = g_queue_pop_head(self->queue);
	GFileInfo *info = g_object_get_data((GObject *) file, "gfileinfo");

	if (info == NULL)
		g_file_query_info_async(file, self->attributes, G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
			self->cancellable, (GAsyncReadyCallback) query_info_cb, self);
	else
	{
		if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
			g_file_enumerate_children_async(file, self->attributes, G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
				self->cancellable, (GAsyncReadyCallback) enumerate_children_cb, self);
		else if (g_file_info_get_file_type(info) == G_FILE_TYPE_REGULAR)
			_gel_beef_run_queue(self);
		else 
		{
			g_warning("Unknow file type");
			_gel_beef_run_queue(self);
		}
	}
}

static void
_gel_beef_run_success(GelBeefOp *self)
{
	if (self->success_cb)
		self->success_cb(self, self->root, self->result, self->data);
}

static void
_gel_beef_run_error(GelBeefOp *self, GFile *source, GError *error)
{
	if (self->error_cb)
		self->error_cb(self, source, error, self->data);
	g_object_unref(source);
	g_error_free(error);
	_gel_beef_run_queue(self);
}

// --
// Querying info
// --
static void
query_info_cb(GFile *source, GAsyncResult *res, GelBeefOp *self)
{
	GError *error = NULL;
	GFileInfo *info = g_file_query_info_finish((GFile *) source, res, &error);
	if (info == NULL)
	{
		_gel_beef_run_error(self, source, error);
		return;
	}
	g_object_set_data((GObject *) source, "gfileinfo", info);

	GFileType type = g_file_info_get_file_type(info);
	if (type == G_FILE_TYPE_DIRECTORY)
	{
		g_file_enumerate_children_async(source, self->attributes, G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
			self->cancellable, (GAsyncReadyCallback) enumerate_children_cb, self);
	}
	else if (type == G_FILE_TYPE_REGULAR)
	{
		_gel_beef_run_queue(self);
	}
	else
	{
		g_printf("Unknow file type\n");
		g_object_unref(info);
		_gel_beef_run_queue(self);
	}
}

// --
// Enumerating children
// --
static void
enumerate_children_cb(GFile *source, GAsyncResult *res, GelBeefOp *self)
{
	GError *error = NULL;
	GFileEnumerator *e = g_file_enumerate_children_finish(source, res, &error);
	if (e == NULL)
	{
		_gel_beef_run_error(self, source, error);
		return;
	}

	g_file_enumerator_next_files_async(e, 8, G_PRIORITY_DEFAULT, self->cancellable, (GAsyncReadyCallback) enumerator_next_cb, self);
}

static void
enumerator_next_cb(GFileEnumerator *e, GAsyncResult *res, GelBeefOp *self)
{
	GError *error = NULL;
	GList *children = g_file_enumerator_next_files_finish(e, res, &error);
	GFile *parent = g_file_enumerator_get_container(e);
	if (error)
	{
		g_file_enumerator_close(e, NULL, NULL);
		_gel_beef_run_error(self, parent, error);
		return;
	}

	if (children == NULL)
	{
		g_file_enumerator_close(e, NULL, NULL);
		_gel_beef_run_queue(self);
		return;
	}

	GList *iter = children;

	GNode *parent_node = g_object_get_data((GObject *) parent, "gnode");
	while (iter)
	{
		GFileInfo *info = (GFileInfo *) iter->data;

		// Create GFile for each children
		GFile *file = g_file_get_child(parent, g_file_info_get_name(info));
		
		// Create GNode for each children
		GNode *node = g_node_new(file);

		// Attach GFileInfo and GNode to GFile
		g_object_set_data((GObject *) file, "gnode", node);
		g_object_set_data((GObject *) file, "gfileinfo", info);

		// Add relation to parent
		g_node_append(parent_node, node);

		// Add this to queue
		g_queue_push_tail(self->queue, file);

		iter = iter->next;
	}
	sort_children(parent_node);
	g_list_free(children);
	g_file_enumerator_next_files_async(e, 8, G_PRIORITY_DEFAULT, self->cancellable, (GAsyncReadyCallback) enumerator_next_cb, self);
}

// --
// Sorting nodes
// --
static void
sort_children(GNode *parent)
{
	GList *l = NULL;
	GNode *iter = parent->children;
	if (iter == NULL)
		return;

	// Unlink each child
	while (iter)
	{
		GNode *next = iter->next;
		g_node_unlink((GNode *) iter);

		l = g_list_prepend(l, iter);
		iter = next;
	}

	l = g_list_sort(l, (GCompareFunc) compare_node_cb);
	GList *i = l;

	while (i)
	{
		g_node_append(parent, (GNode *) i->data);
		i = i->next;
	}
	g_list_free(l);
}

static gint
compare_node_cb(GNode *a, GNode *b)
{
	GFileInfo *_a, *_b;
	_a = g_object_get_data((GObject *) a->data, "gfileinfo");
	_b = g_object_get_data((GObject *) b->data, "gfileinfo");
	g_return_val_if_fail(_a && _b, 0);

	GFileType _ta = g_file_info_get_file_type(_a);
	GFileType _tb = g_file_info_get_file_type(_b);

	if (_ta != _tb)
	{
		if (_ta == G_FILE_TYPE_DIRECTORY)
			return -1;
		else
			return 1;
	}
	else
		return strcmp(g_file_info_get_name(_a), g_file_info_get_name(_b));
}

static gboolean
node_traverse_cb(GNode *node, GList **list)
{
	*list = g_list_prepend(*list, node->data);
	return FALSE;
}

GList *
gel_beef_result_flatten(const GNode *result)
{
	GList *ret = NULL;
	g_node_traverse((GNode *) result, G_PRE_ORDER, G_TRAVERSE_ALL, -1, (GNodeTraverseFunc) node_traverse_cb, &ret);
	return g_list_reverse(ret);
}
