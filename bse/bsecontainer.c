/* BSE - Bedevilled Sound Engine
 * Copyright (C) 1998, 1999 Olaf Hoehmann and Tim Janik
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */
#include	"bsecontainer.h"

#include	"bsesource.h"
#include	"bseproject.h"
#include	"bsestorage.h"
#include	"bseheart.h"
#include	<stdlib.h>
#include	<string.h>



/* --- prototypes --- */
static void	    bse_container_class_init		(BseContainerClass	*class);
static void	    bse_container_init			(BseContainer		*container);
static void	    bse_container_shutdown		(GObject		*object);
static void	    bse_container_finalize		(GObject		*object);
static void	    bse_container_destroy		(BseObject		*object);
static void	    bse_container_store_after		(BseObject		*object,
							 BseStorage		*storage);
static BseTokenType bse_container_try_statement		(BseObject		*object,
							 BseStorage		*storage);
static void	    bse_container_do_add_item		(BseContainer		*container,
							 BseItem		*item);
static void	    bse_container_do_remove_item	(BseContainer		*container,
							 BseItem		*item);
static void         bse_container_prepare               (BseSource              *source,
							 BseIndex                index);
static void         bse_container_reset                 (BseSource              *source);


/* --- variables --- */
static GTypeClass	*parent_class = NULL;
static GQuark		 quark_cross_refs = 0;
static GSList           *containers_cross_changes = NULL;
static guint             containers_cross_changes_handler = 0;


/* --- functions --- */
BSE_BUILTIN_TYPE (BseContainer)
{
  static const GTypeInfo container_info = {
    sizeof (BseContainerClass),

    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) bse_container_class_init,
    (GClassFinalizeFunc) NULL,
    NULL /* class_data */,

    sizeof (BseContainer),
    0 /* n_preallocs */,
    (GInstanceInitFunc) bse_container_init,
  };
  
  return bse_type_register_static (BSE_TYPE_SOURCE,
				   "BseContainer",
				   "Base type to manage BSE items",
				   &container_info);
}

static void
bse_container_class_init (BseContainerClass *class)
{
  GObjectClass *gobject_class;
  BseObjectClass *object_class;
  BseItemClass *item_class;
  BseSourceClass *source_class;
  
  parent_class = g_type_class_peek (BSE_TYPE_SOURCE);
  gobject_class = G_OBJECT_CLASS (class);
  object_class = BSE_OBJECT_CLASS (class);
  item_class = BSE_ITEM_CLASS (class);
  source_class = BSE_SOURCE_CLASS (class);

  if (!quark_cross_refs)
    quark_cross_refs = g_quark_from_static_string ("BseContainerCrossRefs");
  
  gobject_class->shutdown = bse_container_shutdown;
  gobject_class->finalize = bse_container_finalize;

  object_class->store_after = bse_container_store_after;
  object_class->try_statement = bse_container_try_statement;
  object_class->destroy = bse_container_destroy;

  source_class->prepare = bse_container_prepare;
  source_class->reset = bse_container_reset;
  
  class->add_item = bse_container_do_add_item;
  class->remove_item = bse_container_do_remove_item;
  class->forall_items = NULL;
  class->item_seqid = NULL;
  class->get_item = NULL;
}

static void
bse_container_init (BseContainer *container)
{
  container->n_items = 0;
}

static void
bse_container_shutdown (GObject *gobject)
{
  BseContainer *container = BSE_CONTAINER (gobject);

  /* remove any existing cross-references (with notification) */
  bse_object_set_qdata (container, quark_cross_refs, NULL);

  /* chain parent class' shutdown handler */
  G_OBJECT_CLASS (parent_class)->shutdown (gobject);
}

static void
bse_container_destroy (BseObject *object)
{
  BseContainer *container = BSE_CONTAINER (object);

  if (container->n_items)
    g_warning ("%s: shutdown handlers missed to remove %u items",
	       BSE_OBJECT_TYPE_NAME (container),
	       container->n_items);
  
  /* chain parent class' destroy handler */
  BSE_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
bse_container_finalize (GObject *gobject)
{
  /* chain parent class' finalize handler */
  G_OBJECT_CLASS (parent_class)->finalize (gobject);

  /* gobject->finalize() clears the datalist, which may cause this
   * container to end up in the containers_cross_changes list again,
   * so we make sure it is removed *after* the datalist has been
   * cleared. though gobject is an invalid pointer at this time,
   * we can still use it for list removal.
   */
  containers_cross_changes = g_slist_remove_any (containers_cross_changes, gobject);
}

static void
bse_container_do_add_item (BseContainer *container,
			   BseItem	*item)
{
  if (BSE_ITEM_PARENT_REF (item))
    bse_object_ref (BSE_OBJECT (item));
  container->n_items += 1;
  bse_item_set_parent (item, BSE_ITEM (container));

  if (BSE_IS_SOURCE (item) && BSE_SOURCE_PREPARED (container))
    {
      g_return_if_fail (BSE_SOURCE_PREPARED (item) == FALSE);

      BSE_OBJECT_SET_FLAGS (item, BSE_SOURCE_FLAG_PREPARED);
      BSE_SOURCE_GET_CLASS (item)->prepare (BSE_SOURCE (item), bse_heart_get_beat_index ());
    }
}

static inline void
container_add_item (BseContainer *container,
		    BseItem      *item)
{
  bse_object_ref (BSE_OBJECT (container));
  bse_object_ref (BSE_OBJECT (item));

  /* ensure that item names per container are unique
   */
  if (!BSE_OBJECT_NAME (item) || bse_container_lookup_item (container, BSE_OBJECT_NAME (item)))
    {
      gchar *name = BSE_OBJECT_NAME (item);
      gchar *buffer, *p;
      guint i = 0, l;
      
      if (!name)
	name = BSE_OBJECT_TYPE_NAME (item);
      
      l = strlen (name);
      buffer = g_new (gchar, l + 12);
      strcpy (buffer, name);
      p = buffer + l;
      do
	g_snprintf (p, 11, "-%u", ++i);
      while (bse_container_lookup_item (container, buffer));
      
      bse_object_set_name (BSE_OBJECT (item), buffer);
      g_free (buffer);
    }

  BSE_CONTAINER_GET_CLASS (container)->add_item (container, item);
  BSE_NOTIFY (container, item_added, NOTIFY (OBJECT, item, DATA));

  bse_object_unref (BSE_OBJECT (item));
  bse_object_unref (BSE_OBJECT (container));
}

void
bse_container_add_item (BseContainer *container,
			BseItem      *item)
{
  g_return_if_fail (BSE_IS_CONTAINER (container));
  /* FIXME: g_return_if_fail (!BSE_OBJECT_DESTROYED (container)); */
  g_return_if_fail (BSE_IS_ITEM (item));
  g_return_if_fail (item->parent == NULL);
  g_return_if_fail (BSE_CONTAINER_GET_CLASS (container)->add_item != NULL); /* paranoid */

  BSE_OBJECT_SET_FLAGS (item, BSE_ITEM_FLAG_PARENT_REF);
  container_add_item (container, item);
}

void
bse_container_add_item_unrefed (BseContainer *container,
				BseItem      *item)
{
  g_return_if_fail (BSE_IS_CONTAINER (container));
  /* FIXME: g_return_if_fail (!BSE_OBJECT_DESTROYED (container)); */
  g_return_if_fail (BSE_IS_ITEM (item));
  g_return_if_fail (item->parent == NULL);
  /* FIXME: g_return_if_fail (!BSE_OBJECT_DESTROYED (item)); */
  g_return_if_fail (BSE_CONTAINER_GET_CLASS (container)->add_item != NULL); /* paranoid */

  /* we don't want the item to stay around, due to *our* ref count,
   * we get notified about item destruction through item's shutdown method
   */

  BSE_OBJECT_UNSET_FLAGS (item, BSE_ITEM_FLAG_PARENT_REF);
  container_add_item (container, item);
}

BseItem*
bse_container_new_item (BseContainer *container,
			GType         item_type,
			const gchar  *first_param_name,
			...)
{
  BseItem *item;
  va_list var_args;

  g_return_val_if_fail (BSE_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (g_type_is_a (item_type, BSE_TYPE_ITEM), NULL);

  va_start (var_args, first_param_name);
  item = bse_object_new_valist (item_type, first_param_name, var_args);
  va_end (var_args);
  bse_container_add_item (container, item);
  bse_object_unref (BSE_OBJECT (item));

  return item;
}

static void
bse_container_do_remove_item (BseContainer *container,
			      BseItem	   *item)
{
  BseItem *anchestor = BSE_ITEM (container);

  container->n_items -= 1;

  do
    {
      bse_container_uncross_item (BSE_CONTAINER (anchestor), item);
      anchestor = anchestor->parent;
    }
  while (anchestor);

  if (BSE_IS_SOURCE (item) && BSE_SOURCE_PREPARED (container))
    {
      g_return_if_fail (BSE_SOURCE_PREPARED (item) == TRUE);

      bse_source_reset (BSE_SOURCE (item));

      if (!BSE_IS_CONTAINER (item))
	{
	  bse_source_clear_ichannels (BSE_SOURCE (item));
	  bse_source_clear_ochannels (BSE_SOURCE (item));
	}
    }

  /* reset parent *after* uncrossing, so "set_parent" notifiers on item
   * operate on sane object trees
   */
  bse_item_set_parent (item, NULL);

  if (BSE_ITEM_PARENT_REF (item))
    {
      bse_object_unref (BSE_OBJECT (item));
      BSE_OBJECT_UNSET_FLAGS (item, BSE_ITEM_FLAG_PARENT_REF);
    }
}

void
bse_container_remove_item (BseContainer *container,
			   BseItem      *item)
{
  g_return_if_fail (BSE_IS_CONTAINER (container));
  g_return_if_fail (BSE_IS_ITEM (item));
  g_return_if_fail (item->parent == BSE_ITEM (container));
  g_return_if_fail (BSE_CONTAINER_GET_CLASS (container)->remove_item != NULL); /* paranoid */

  bse_object_ref (BSE_OBJECT (container));
  bse_object_ref (BSE_OBJECT (item));

  BSE_CONTAINER_GET_CLASS (container)->remove_item (container, item);
  BSE_NOTIFY (container, item_removed, NOTIFY (OBJECT, item, DATA));

  bse_object_unref (BSE_OBJECT (item));
  bse_object_unref (BSE_OBJECT (container));
}

void
bse_container_forall_items (BseContainer      *container,
			    BseForallItemsFunc func,
			    gpointer           data)
{
  g_return_if_fail (BSE_IS_CONTAINER (container));
  g_return_if_fail (func != NULL);

  if (container->n_items)
    {
      g_return_if_fail (BSE_CONTAINER_GET_CLASS (container)->forall_items != NULL); /* paranoid */

      BSE_CONTAINER_GET_CLASS (container)->forall_items (container, func, data);
    }
}

static gboolean
list_items (BseItem *item,
	    gpointer data)
{
  GList **list_p = data;

  *list_p = g_list_prepend (*list_p, item);

  return TRUE;
}

GList*
bse_container_list_items (BseContainer *container)
{
  g_return_val_if_fail (BSE_IS_CONTAINER (container), NULL);

  if (container->n_items)
    {
      GList *list = NULL;

      g_return_val_if_fail (BSE_CONTAINER_GET_CLASS (container)->forall_items != NULL, NULL); /* paranoid */

      BSE_CONTAINER_GET_CLASS (container)->forall_items (container, list_items, &list);

      return list;
    }
  else
    return NULL;
}

static gboolean
count_item_seqid (BseItem *item,
		  gpointer data_p)
{
  gpointer *data = data_p;
  
  data[1] = GUINT_TO_POINTER (GPOINTER_TO_UINT (data[1]) + 1);
  
  return item != data[0];
}

guint
bse_container_get_item_seqid (BseContainer *container,
			      BseItem      *item)
{
  g_return_val_if_fail (BSE_IS_CONTAINER (container), 0);
  g_return_val_if_fail (BSE_IS_ITEM (item), 0);
  g_return_val_if_fail (item->parent == BSE_ITEM (container), 0);
  
  if (BSE_CONTAINER_GET_CLASS (container)->item_seqid)
    return BSE_CONTAINER_GET_CLASS (container)->item_seqid (container, item);
  else if (container->n_items)
    {
      gpointer data[2];
      
      g_return_val_if_fail (BSE_CONTAINER_GET_CLASS (container)->forall_items != NULL, 0); /* paranoid */
      
      data[0] = item;
      data[1] = GUINT_TO_POINTER (0);
      
      BSE_CONTAINER_GET_CLASS (container)->forall_items (container, count_item_seqid, data);

      return GPOINTER_TO_UINT (data[1]);
    }
  else
    return 0;
}

static gboolean
find_nth_item (BseItem *item,
	       gpointer data_p)
{
  gpointer *data = data_p;
  guint n = GPOINTER_TO_UINT (data[1]);

  n -= 1;
  data[1] = GUINT_TO_POINTER (n);

  if (!n)
    {
      data[0] = item;

      return FALSE;
    }
  else
    return TRUE;
}

BseItem*
bse_container_get_item (BseContainer *container,
			GType         item_type,
			guint         seqid)
{
  g_return_val_if_fail (BSE_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (seqid > 0, NULL);
  g_return_val_if_fail (g_type_is_a (item_type, BSE_TYPE_ITEM), NULL);

  if (BSE_CONTAINER_GET_CLASS (container)->get_item)
    return BSE_CONTAINER_GET_CLASS (container)->get_item (container, item_type, seqid);
  else if (container->n_items)
    {
      gpointer data[2];

      g_return_val_if_fail (BSE_CONTAINER_GET_CLASS (container)->forall_items != NULL, NULL); /* paranoid */

      data[0] = NULL;
      data[1] = GUINT_TO_POINTER (seqid);

      BSE_CONTAINER_GET_CLASS (container)->forall_items (container, find_nth_item, data);

      return data[0];
    }
  else
    return NULL;
}

static gboolean
store_forall (BseItem *item,
	      gpointer data_p)
{
  gpointer *data = data_p;
  BseContainer *container = data[0];
  BseStorage *storage = data[1];
  gchar *path;

  bse_storage_break (storage);
  bse_storage_putc (storage, '(');

  path = bse_container_make_item_path (container, item, TRUE);
  bse_storage_puts (storage, path);
  g_free (path);

  bse_storage_push_level (storage);
  bse_object_store (BSE_OBJECT (item), storage);
  bse_storage_pop_level (storage);
  
  return TRUE;
}

void
bse_container_store_items (BseContainer *container,
			   BseStorage   *storage)
{
  gpointer data[2];

  g_return_if_fail (BSE_IS_CONTAINER (container));
  g_return_if_fail (BSE_IS_STORAGE (storage));

  bse_object_ref (BSE_OBJECT (container));
  data[0] = container;
  data[1] = storage;
  bse_container_forall_items (container, store_forall, data);
  bse_object_unref (BSE_OBJECT (container));
}

static void
bse_container_store_after (BseObject  *object,
			   BseStorage *storage)
{
  /* we *append* items to our normal container stuff, so they
   * come _after_ private stuff, stored by derived containers
   * (which usually store their stuff through store_private())
   */
  bse_container_store_items (BSE_CONTAINER (object), storage);

  /* chain parent class' handler */
  if (BSE_OBJECT_CLASS (parent_class)->store_after)
    BSE_OBJECT_CLASS (parent_class)->store_after (object, storage);
}

static BseTokenType
bse_container_try_statement (BseObject  *object,
			     BseStorage *storage)
{
  GScanner *scanner = storage->scanner;
  GTokenType expected_token;

  /* chain parent class' handler */
  expected_token = BSE_OBJECT_CLASS (parent_class)->try_statement (object, storage);

  /* pretty much the only valid reason to overload try_statement() is for this
   * case, where we attempt to parse items as a last resort
   */
  if (expected_token == BSE_TOKEN_UNMATCHED &&
      g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER)
    {
      BseItem *item;

      item = bse_container_item_from_path (BSE_CONTAINER (object),
					   scanner->next_value.v_identifier);
      if (item)
	{
	  g_scanner_get_next_token (scanner); /* eat the path */

	  /* now let the item restore itself
	   */
	  expected_token = bse_object_restore (BSE_OBJECT (item), storage);
	}
    }

  return expected_token;
}

static gboolean
find_named_item (BseItem *item,
		 gpointer data_p)
{
  gpointer *data = data_p;
  gchar *name = data[1];

  if (bse_string_equals (BSE_OBJECT_NAME (item), name))
    {
      data[0] = item;
      return FALSE;
    }
  return TRUE;
}

BseItem*
bse_container_lookup_item (BseContainer *container,
			   const gchar  *name)
{
  gpointer data[2] = { NULL, };

  g_return_val_if_fail (BSE_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  /* FIXME: better use a hashtable here */
  
  data[1] = (gpointer) name;
  bse_container_forall_items (container, find_named_item, data);

  return data[0];
}

static gboolean
find_named_typed_item (BseItem *item,
		       gpointer data_p)
{
  gpointer *data = data_p;
  gchar *name = data[1];
  GType   type = GPOINTER_TO_UINT (data[2]);

  if (g_type_is_a (BSE_OBJECT_TYPE (item), type) &&
      bse_string_equals (BSE_OBJECT_NAME (item), name))
    {
      data[0] = item;
      return FALSE;
    }
  return TRUE;
}

BseItem*
bse_container_item_from_handle (BseContainer *container,
				const gchar  *handle)
{
  gchar *type_name, *ident, *name = NULL;
  BseItem *item = NULL;
  GType   type;

  g_return_val_if_fail (BSE_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (handle != NULL, NULL);

  /* handle syntax:
   * <TYPE> [ <:> { <SeqId> | <:> <NAME> } ]
   * examples:
   * "BseItem"	     generic handle    -> create item of type BseItem
   * "BseItem:3"     sequential handle -> get third item of type BseItem
   * "BseItem::foo"  named handle      -> create/get item of type BseItem, named "foo"
   *
   * to get unique matches for named handles, items of a specific
   * container need to have unique names (enforced in bse_container_add_item()
   * and bse_item_do_set_name()).
   */

  type_name = g_strdup (handle);
  ident = strchr (type_name, ':');
  if (ident)
    {
      *(ident++) = 0;
      if (*ident == ':')
	name = ident + 1;
    }
  type = g_type_from_name (type_name);
  if (g_type_is_a (type, BSE_TYPE_ITEM))
    {
      if (name)
	{
	  gpointer data[3] = { NULL, };
	  
	  data[1] = name;
	  data[2] = GUINT_TO_POINTER (type);
	  bse_container_forall_items (container, find_named_typed_item, data);
	  item = data[0];

	  if (!item)
	    item = bse_container_new_item (container, type, "name", name, NULL);
	}
      else if (ident)
	{
	  gchar *f = NULL;
	  guint seq_id = strtol (ident, &f, 10);
	  
	  if (seq_id > 0 && (!f || *f == 0))
	    item = bse_container_get_item (container, type, seq_id);
	}
      else
	item = bse_container_new_item (container, type, NULL);
    }
  g_free (type_name);

  return item;
}

BseItem*
bse_container_item_from_path (BseContainer *container,
			      const gchar  *path)
{
  gchar *handle, *next_handle;
  BseItem *item = NULL;

  g_return_val_if_fail (BSE_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  handle = g_strdup (path);
  next_handle = strchr (handle, '.');
  if (next_handle)
    {
      *(next_handle++) = 0;
      item = bse_container_item_from_handle (container, handle);
      if (BSE_IS_CONTAINER (item))
	item = bse_container_item_from_handle (BSE_CONTAINER (item), next_handle);
      else
	item = NULL;
    }
  else
    item = bse_container_item_from_handle (container, handle);
  g_free (handle);

  return item;
}

gchar* /* free result */
bse_container_make_item_path (BseContainer *container,
			      BseItem      *item,
			      gboolean      persistent)
{
  gchar *path;

  g_return_val_if_fail (BSE_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (BSE_IS_ITEM (item), NULL);
  g_return_val_if_fail (bse_item_has_ancestor (item, BSE_ITEM (container)), NULL);

  path = bse_item_make_handle (item, persistent);

  while (item->parent != BSE_ITEM (container))
    {
      gchar *string, *handle;

      item = item->parent;
      string = path;
      handle = bse_item_make_handle (item, persistent);
      path = g_strconcat (handle, ".", string, NULL);
      g_free (string);
      g_free (handle);
    }

  return path;
}

static gboolean
notify_cross_changes (gpointer data)
{
  while (containers_cross_changes)
    {
      BseContainer *container = BSE_CONTAINER (containers_cross_changes->data);

      containers_cross_changes = g_slist_remove_any (containers_cross_changes, container);
      BSE_NOTIFY (container, cross_changes, NOTIFY (OBJECT, DATA));
    }

  containers_cross_changes_handler = 0;

  return FALSE;
}

static inline void
container_queue_cross_changes (BseContainer *container)
{
  if (!containers_cross_changes_handler)
    containers_cross_changes_handler = g_idle_add_full (BSE_NOTIFY_PRIORITY,
							notify_cross_changes,
							NULL,
							NULL);
  containers_cross_changes = g_slist_prepend (containers_cross_changes, container);
}

typedef struct _BseContainerCrossRefs BseContainerCrossRefs;
typedef struct
{
  BseItem         *owner;
  BseItem         *ref_item;
  BseItemCrossFunc destroy;
  gpointer         data;
} CrossRef;
struct _BseContainerCrossRefs
{
  guint         n_cross_refs;
  BseContainer *container;
  CrossRef      cross_refs[1]; /* flexible array */
};

static inline void
destroy_cref (BseContainerCrossRefs *crefs,
	      guint                  n)
{
  BseItem *owner = crefs->cross_refs[n].owner;
  BseItem *ref_item = crefs->cross_refs[n].ref_item;
  BseItemCrossFunc destroy = crefs->cross_refs[n].destroy;
  gpointer data = crefs->cross_refs[n].data;

  crefs->n_cross_refs--;
  if (n < crefs->n_cross_refs)
    crefs->cross_refs[n] = crefs->cross_refs[crefs->n_cross_refs];

  destroy (owner, ref_item, data);
}

static void
destroy_crefs (gpointer data)
{
  BseContainerCrossRefs *crefs = data;

  if (crefs->n_cross_refs)
    container_queue_cross_changes (crefs->container);

  while (crefs->n_cross_refs)
    destroy_cref (crefs, crefs->n_cross_refs - 1);
  g_free (crefs);
}

static inline void
container_set_crefs (gpointer               container,
		     BseContainerCrossRefs *crefs)
{
  bse_object_steal_qdata (container, quark_cross_refs);
  bse_object_set_qdata_full (container, quark_cross_refs, crefs, destroy_crefs);
}

static inline BseContainerCrossRefs*
container_get_crefs (gpointer container)
{
  return bse_object_get_qdata (container, quark_cross_refs);
}

void
bse_container_cross_ref (BseContainer    *container,
			 BseItem         *owner,
			 BseItem         *ref_item,
			 BseItemCrossFunc destroy_func,
			 gpointer         data)
{
  BseContainerCrossRefs *crefs;
  guint i;

  /* prerequisites:
   * container == bse_item_common_ancestor (owner, ref_item)
   * container != owner || container != ref_item
   */

  g_return_if_fail (BSE_IS_CONTAINER (container));
  g_return_if_fail (BSE_IS_ITEM (owner));
  g_return_if_fail (BSE_IS_ITEM (ref_item));
  g_return_if_fail (destroy_func != NULL);

  crefs = container_get_crefs (container);
  if (!crefs)
    {
      i = 0;
      crefs = g_realloc (crefs, sizeof (BseContainerCrossRefs));
      crefs->n_cross_refs = i + 1;
      crefs->container = container;
      container_set_crefs (container, crefs);
    }
  else
    {
      BseContainerCrossRefs *old_loc = crefs;

      i = crefs->n_cross_refs++;
      crefs = g_realloc (crefs, sizeof (BseContainerCrossRefs) + i * sizeof (crefs->cross_refs[0]));
      if (old_loc != crefs)
	container_set_crefs (container, crefs);
    }
  crefs->cross_refs[i].owner = owner;
  crefs->cross_refs[i].ref_item = ref_item;
  crefs->cross_refs[i].destroy = destroy_func;
  crefs->cross_refs[i].data = data;

  container_queue_cross_changes (container);
}

void
bse_container_cross_unref (BseContainer *container,
			   BseItem      *owner,
			   BseItem      *ref_item)
{
  BseContainerCrossRefs *crefs;
  gboolean found_one = FALSE;
  
  g_return_if_fail (BSE_IS_CONTAINER (container));
  g_return_if_fail (BSE_IS_ITEM (owner));
  g_return_if_fail (BSE_IS_ITEM (ref_item));

  bse_object_ref (BSE_OBJECT (container));
  bse_object_ref (BSE_OBJECT (owner));
  bse_object_ref (BSE_OBJECT (ref_item));

  crefs = container_get_crefs (container);
  if (crefs)
    {
      guint i = 0;

      while (i < crefs->n_cross_refs)
	{
	  if (crefs->cross_refs[i].owner == owner &&
	      crefs->cross_refs[i].ref_item == ref_item)
	    {
	      destroy_cref (crefs, i);
	      container_queue_cross_changes (container);
	      found_one = TRUE;
	      break;
	    }
	  i++;
	}
    }

  if (!found_one)
    g_warning (G_STRLOC ": unable to find cross ref from `%s' to `%s' on `%s'",
	       BSE_OBJECT_TYPE_NAME (owner),
	       BSE_OBJECT_TYPE_NAME (ref_item),
	       BSE_OBJECT_TYPE_NAME (container));

  bse_object_unref (BSE_OBJECT (ref_item));
  bse_object_unref (BSE_OBJECT (owner));
  bse_object_unref (BSE_OBJECT (container));
}

/* we could in theory use bse_item_has_ancestor() here,
 * but actually this test should be as fast as possible,
 * and we also want to catch item == container
 */
static inline gboolean
item_check_branch (BseItem *item,
		   gpointer container)
{
  BseItem *anchestor = container;

  do
    {
      if (item == anchestor)
	return TRUE;
      item = item->parent;
    }
  while (item);

  return FALSE;
}

void
bse_container_uncross_item (BseContainer *container,
			    BseItem      *item)
{
  BseContainerCrossRefs *crefs;
  gboolean found_one = FALSE;

  /* prerequisites:
   * bse_item_has_ancestor (item, container) == TRUE
   */

  g_return_if_fail (BSE_IS_CONTAINER (container));
  g_return_if_fail (BSE_IS_ITEM (item));

  crefs = container_get_crefs (container);
  if (crefs)
    {
      guint i = 0;
      
      bse_object_ref (BSE_OBJECT (container));
      bse_object_ref (BSE_OBJECT (item));

      /* suppress tree walks where possible
       */
      if (!BSE_IS_CONTAINER (item) || ((BseContainer*) item)->n_items == 0)
	while (i < crefs->n_cross_refs)
	  {
	    if (crefs->cross_refs[i].owner == item || crefs->cross_refs[i].ref_item == item)
	      {
		found_one = TRUE;
		destroy_cref (crefs, i);
	      }
	    else
	      i++;
	  }
      else /* need to check whether item is anchestor of any of the cross-ref items here */
	{
	  BseItem *saved_parent, *citem = BSE_ITEM (container);

	  /* we do some minor hackery here, for optimization purposes:
	   * since item is a child of container, we don't need to walk
	   * ->owner's or ->ref_item's anchestor list any further than
	   * up to reaching container.
	   * to suppress extra checks in item_check_branch() in this
	   * regard, we simply set container->parent to NULL temporarily
	   * and with that cause item_check_branch() to abort automatically
	   */
	  saved_parent = citem->parent;
	  citem->parent = NULL;
	  while (i < crefs->n_cross_refs)
	    {
	      if (item_check_branch (crefs->cross_refs[i].owner, item) ||
		  item_check_branch (crefs->cross_refs[i].ref_item, item))
		{
		  citem->parent = saved_parent;

		  found_one = TRUE;
		  destroy_cref (crefs, i);

		  saved_parent = citem->parent;
		  citem->parent = NULL;
		}
	      else
		i++;
	    }
	  citem->parent = saved_parent;
	}

      if (found_one)
	container_queue_cross_changes (container);

      bse_object_unref (BSE_OBJECT (item));
      bse_object_unref (BSE_OBJECT (container));
    }
}

void
bse_container_cross_forall (BseContainer      *container,
			    BseForallCrossFunc func,
			    gpointer           data)
{
  BseContainerCrossRefs *crefs;

  g_return_if_fail (BSE_IS_CONTAINER (container));
  g_return_if_fail (func != NULL);

  crefs = container_get_crefs (container);
  if (crefs)
    {
      guint i;

      for (i = 0; i < crefs->n_cross_refs; i++)
	if (!func (crefs->cross_refs[i].owner, crefs->cross_refs[i].ref_item, data))
	  return;
    }
}

static gboolean
prepare_item (BseItem *item,
	      gpointer data)
{
  if (BSE_IS_SOURCE (item) && !BSE_SOURCE_PREPARED (item))
    {
      BSE_OBJECT_SET_FLAGS (item, BSE_SOURCE_FLAG_PREPARED);
      BSE_SOURCE_GET_CLASS (item)->prepare (BSE_SOURCE (item), bse_heart_get_beat_index ());
    }

  return TRUE;
}

static void
bse_container_prepare (BseSource *source,
		       BseIndex   index)
{
  BseContainer *container = BSE_CONTAINER (source);

  /* chain parent class' handler */
  BSE_SOURCE_CLASS (parent_class)->prepare (source, index);

  /* make sure all BseSource children are prepared as well */
  if (container->n_items)
    {
      g_return_if_fail (BSE_CONTAINER_GET_CLASS (container)->forall_items != NULL); /* paranoid */

      BSE_CONTAINER_GET_CLASS (container)->forall_items (container, prepare_item, NULL);
    }
}

static gboolean
reset_item (BseItem *item,
	    gpointer data)
{
  if (BSE_IS_SOURCE (item))
    {
      g_return_val_if_fail (BSE_SOURCE_PREPARED (item), TRUE);

      bse_source_reset (BSE_SOURCE (item));
    }

  return TRUE;
}

static void
bse_container_reset (BseSource *source)
{
  BseContainer *container = BSE_CONTAINER (source);

  /* make sure all BseSource children are reset as well */
  if (container->n_items)
    {
      g_return_if_fail (BSE_CONTAINER_GET_CLASS (container)->forall_items != NULL); /* paranoid */

      BSE_CONTAINER_GET_CLASS (container)->forall_items (container, reset_item, NULL);
    }

  /* chain parent class' handler */
  BSE_SOURCE_CLASS (parent_class)->reset (source);
}
