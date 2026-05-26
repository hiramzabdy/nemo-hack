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

#ifndef NEMO_EXPLORER_SIDEBAR_H
#define NEMO_EXPLORER_SIDEBAR_H

#include <gtk/gtk.h>
#include "nemo-window.h"

G_BEGIN_DECLS

#define NEMO_TYPE_EXPLORER_SIDEBAR (nemo_explorer_sidebar_get_type ())
G_DECLARE_FINAL_TYPE (NemoExplorerSidebar, nemo_explorer_sidebar, NEMO, EXPLORER_SIDEBAR, GtkBox)

GtkWidget * nemo_explorer_sidebar_new (NemoWindow *window);

G_END_DECLS

#endif /* NEMO_EXPLORER_SIDEBAR_H */
