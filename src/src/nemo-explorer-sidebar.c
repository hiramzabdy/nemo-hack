/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nemo Explorer Sidebar
 *
 *  Combined places + tree sidebar, like Windows Explorer's
 *  navigation pane.
 *
 *  Architecture: A single GtkScrolledWindow contains both the
 *  NemoPlacesSidebar (bookmarks, devices, network) at the top
 *  and the FMTreeView (directory tree) below a separator.
 *  Both child widgets have their internal scrolling disabled
 *  (GTK_POLICY_NEVER) so the sidebar scrolls as one unified
 *  panel — no double scrollbars.
 *
 *  Signal note: Both child widgets independently subscribe to
 *  NemoWindow signals (loading_uri, volume changes, etc.).
 *  This is intentional — the tree should expand to show the
 *  current folder regardless of whether navigation was
 *  initiated from places or tree. GTK's main loop serializes
 *  callbacks, so there is no race condition.
 *
 *  This file is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 */

#include <config.h>

#include "nemo-explorer-sidebar.h"
#include "nemo-places-sidebar.h"
#include "nemo-tree-sidebar.h"

struct _NemoExplorerSidebar {
	GtkBox parent_instance;
};

typedef struct {
	NemoWindow *window;
	GtkWidget  *places_sidebar;
	GtkWidget  *tree_sidebar;
	GtkWidget  *content_box;
} NemoExplorerSidebarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NemoExplorerSidebar, nemo_explorer_sidebar, GTK_TYPE_BOX);

static void
nemo_explorer_sidebar_dispose (GObject *object)
{
	NemoExplorerSidebar *sidebar = NEMO_EXPLORER_SIDEBAR (object);
	NemoExplorerSidebarPrivate *priv =
		nemo_explorer_sidebar_get_instance_private (sidebar);

	/* Child widgets are destroyed automatically via GTK's
	 * parent-child cascade when the content_box is destroyed.
	 * Null the pointers to avoid dangling references. */
	g_clear_object (&priv->places_sidebar);
	g_clear_object (&priv->tree_sidebar);

	G_OBJECT_CLASS (nemo_explorer_sidebar_parent_class)->dispose (object);
}

static void
nemo_explorer_sidebar_class_init (NemoExplorerSidebarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = nemo_explorer_sidebar_dispose;
}

static void
nemo_explorer_sidebar_init (NemoExplorerSidebar *sidebar)
{
	NemoExplorerSidebarPrivate *priv;
	GtkWidget *scrolled;

	priv = nemo_explorer_sidebar_get_instance_private (sidebar);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (sidebar),
	                                GTK_ORIENTATION_VERTICAL);

	gtk_style_context_add_class (
		gtk_widget_get_style_context (GTK_WIDGET (sidebar)),
		"nemo-explorer-sidebar");

	/* Single scrolled window wraps both places and tree.
	 * Horizontal: NEVER (no horizontal scroll — child width
	 *   is constrained by the sidebar allocation).
	 * Vertical: AUTOMATIC (scrollbar appears when content
	 *   exceeds visible height). */
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (sidebar), scrolled, TRUE, TRUE, 0);

	/* Content box: places → separator → tree */
	priv->content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (scrolled), priv->content_box);
}

GtkWidget *
nemo_explorer_sidebar_new (NemoWindow *window)
{
	NemoExplorerSidebar *sidebar;
	NemoExplorerSidebarPrivate *priv;
	GtkWidget *places, *tree, *separator;

	sidebar = g_object_new (NEMO_TYPE_EXPLORER_SIDEBAR, NULL);
	priv = nemo_explorer_sidebar_get_instance_private (sidebar);
	priv->window = window;

	/* ── Places section (top, fixed height) ── */
	places = nemo_places_sidebar_new (window);

	/* Disable internal scrolling so the outer scrolled window
	 * handles all scrolling. Remove the inset shadow to avoid
	 * double borders. */
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (places),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_NEVER);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (places),
	                                     GTK_SHADOW_NONE);

	gtk_box_pack_start (GTK_BOX (priv->content_box), places,
	                    FALSE, FALSE, 0);
	priv->places_sidebar = places;

	/* ── Separator ── */
	separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (priv->content_box), separator,
	                    FALSE, FALSE, 4);

	/* ── Tree section (bottom, expands to fill remaining space) ── */
	tree = nemo_tree_sidebar_new (window);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tree),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_NEVER);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (tree),
	                                     GTK_SHADOW_NONE);

	gtk_box_pack_start (GTK_BOX (priv->content_box), tree,
	                    TRUE, TRUE, 0);
	priv->tree_sidebar = tree;

	gtk_widget_show_all (GTK_WIDGET (sidebar));

	return GTK_WIDGET (sidebar);
}
