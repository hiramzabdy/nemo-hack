/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nemo Explorer Sidebar
 *
 *  Combined places + tree sidebar, like Windows Explorer's
 *  navigation pane.
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
	GtkWidget *places_sidebar;
	GtkWidget *tree_sidebar;
} NemoExplorerSidebarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NemoExplorerSidebar, nemo_explorer_sidebar, GTK_TYPE_BOX);

static void
nemo_explorer_sidebar_dispose (GObject *object)
{
	NemoExplorerSidebar *sidebar = NEMO_EXPLORER_SIDEBAR (object);
	NemoExplorerSidebarPrivate *priv = nemo_explorer_sidebar_get_instance_private (sidebar);

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
	gtk_orientable_set_orientation (GTK_ORIENTABLE (sidebar), GTK_ORIENTATION_VERTICAL);
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (sidebar)),
				     "nemo-explorer-sidebar");
}

GtkWidget *
nemo_explorer_sidebar_new (NemoWindow *window)
{
	NemoExplorerSidebar *sidebar;
	NemoExplorerSidebarPrivate *priv;
	GtkWidget *paned;

	sidebar = g_object_new (NEMO_TYPE_EXPLORER_SIDEBAR, NULL);
	priv = nemo_explorer_sidebar_get_instance_private (sidebar);
	priv->window = window;

	paned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start (GTK_BOX (sidebar), paned, TRUE, TRUE, 0);
	gtk_widget_show (paned);

	priv->places_sidebar = nemo_places_sidebar_new (window);
	gtk_paned_pack1 (GTK_PANED (paned), priv->places_sidebar,
			 FALSE, FALSE);
	gtk_widget_show (priv->places_sidebar);

	priv->tree_sidebar = nemo_tree_sidebar_new (window);
	gtk_paned_pack2 (GTK_PANED (paned), priv->tree_sidebar,
			 TRUE, FALSE);
	gtk_widget_show (priv->tree_sidebar);

	gtk_widget_show (GTK_WIDGET (sidebar));

	return GTK_WIDGET (sidebar);
}
