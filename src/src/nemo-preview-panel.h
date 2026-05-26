/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nemo Preview Panel
 *
 *  Inline file preview widget for the right side of Nemo window.
 *  Mimics Windows Explorer's preview pane (Alt+P).
 *
 *  This file is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 */

#ifndef __NEMO_PREVIEW_PANEL_H__
#define __NEMO_PREVIEW_PANEL_H__

#include <gtk/gtk.h>
#include <libnemo-private/nemo-file.h>

G_BEGIN_DECLS

#define NEMO_TYPE_PREVIEW_PANEL nemo_preview_panel_get_type()
#define NEMO_PREVIEW_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PREVIEW_PANEL, NemoPreviewPanel))
#define NEMO_PREVIEW_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PREVIEW_PANEL, NemoPreviewPanelClass))
#define NEMO_IS_PREVIEW_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PREVIEW_PANEL))
#define NEMO_IS_PREVIEW_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PREVIEW_PANEL))
#define NEMO_PREVIEW_PANEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PREVIEW_PANEL, NemoPreviewPanelClass))

typedef struct _NemoPreviewPanelPrivate NemoPreviewPanelPrivate;

typedef struct {
  GtkBox parent;
  NemoPreviewPanelPrivate *priv;
} NemoPreviewPanel;

typedef struct {
  GtkBoxClass parent_class;
} NemoPreviewPanelClass;

GType              nemo_preview_panel_get_type    (void);
GtkWidget *        nemo_preview_panel_new         (void);
void               nemo_preview_panel_set_file    (NemoPreviewPanel *panel,
                                                    NemoFile         *file);
void               nemo_preview_panel_clear       (NemoPreviewPanel *panel);
void               nemo_preview_panel_rescale     (NemoPreviewPanel *panel);

G_END_DECLS

#endif /* __NEMO_PREVIEW_PANEL_H__ */
