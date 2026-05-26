/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nemo Preview Panel — inline file preview widget
 *
 *  Displays a preview of the selected file in the right panel
 *  of Nemo, similar to Windows Explorer's preview pane.
 *
 *  Copyright (C) 2024
 *
 *  Nemo is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nemo is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 */

#include <config.h>
#include "nemo-preview-panel.h"

#include <glib/gi18n.h>
#include <string.h>

#define MAX_TEXT_PREVIEW_BYTES  16384   /* ~16 KB of text preview */
#define MAX_IMAGE_FILE_SIZE     (50ULL * 1024 * 1024)  /* 50 MB cap */
#define PREVIEW_ICON_LARGE_SIZE 128
#define PREVIEW_DEFAULT_WIDTH   280
#define PREVIEW_INTERNAL_PADDING 16

struct _NemoPreviewPanelPrivate {
    GtkWidget *icon_image;
    GtkWidget *name_label;
    GtkWidget *type_label;
    GtkWidget *separator;
    GtkWidget *preview_image;     /* GtkImage for image/icon preview */
    GtkWidget *text_scrolled;     /* GtkScrolledWindow for text preview */
    GtkWidget *text_view;         /* GtkTextView for text files */
    GtkWidget *metadata_grid;     /* GtkGrid for file metadata */
    GtkWidget *placeholder_label; /* "Select a file to preview" */

    /* Metadata value labels (updated in-place) */
    GtkWidget *meta_type_val;
    GtkWidget *meta_size_val;
    GtkWidget *meta_location_val;
    GtkWidget *meta_info_val;

    GdkPixbuf *full_pixbuf;       /* full-resolution image (owned) */
    gint       last_scaled_width; /* avoid redundant rescale */

    NemoFile *current_file;
};

G_DEFINE_TYPE_WITH_PRIVATE (NemoPreviewPanel, nemo_preview_panel, GTK_TYPE_BOX);

/* ── Image scaling helper ────────────────────────────────────── */

/* Scale a pixbuf to fit a target width, maintaining aspect ratio.
 * Never upscales beyond native resolution.
 * Returns a new pixbuf (caller must unref), or NULL on failure. */
static GdkPixbuf *
scale_pixbuf_to_width (GdkPixbuf *src, gint target_w)
{
    gint src_w, src_h;
    gint scaled_h;
    gdouble scale;

    src_w = gdk_pixbuf_get_width (src);
    src_h = gdk_pixbuf_get_height (src);

    if (target_w > src_w)
        target_w = src_w;

    scale = (gdouble) target_w / (gdouble) src_w;
    scaled_h = (gint) (src_h * scale);

    if (scaled_h < 1)
        scaled_h = 1;

    return gdk_pixbuf_scale_simple (src, target_w, scaled_h,
                                    GDK_INTERP_BILINEAR);
}

/* ── GObject lifecycle ───────────────────────────────────────── */

static void
nemo_preview_panel_dispose (GObject *object)
{
    NemoPreviewPanel *panel = NEMO_PREVIEW_PANEL (object);

    g_clear_object (&panel->priv->full_pixbuf);

    if (panel->priv->current_file != NULL) {
        g_object_unref (panel->priv->current_file);
        panel->priv->current_file = NULL;
    }

    G_OBJECT_CLASS (nemo_preview_panel_parent_class)->dispose (object);
}

/* ── Metadata grid (created once, updated in-place) ──────────── */

/* Add a metadata row: label on left, value on right.
 * Returns the value label so the caller can update it later. */
static GtkWidget *
add_metadata_row (NemoPreviewPanel *panel, GtkWidget *grid,
                  const gchar *label_text, const gchar *value_text, gint row)
{
    GtkWidget *label;
    GtkWidget *value;

    label = gtk_label_new (label_text);
    gtk_widget_set_halign (label, GTK_ALIGN_END);
    gtk_widget_set_hexpand (label, FALSE);
    gtk_label_set_xalign (GTK_LABEL (label), 1.0f);

    value = gtk_label_new (value_text ? value_text : _("Unknown"));
    gtk_widget_set_halign (value, GTK_ALIGN_START);
    gtk_widget_set_hexpand (value, TRUE);
    gtk_label_set_xalign (GTK_LABEL (value), 0.0f);
    gtk_label_set_ellipsize (GTK_LABEL (value), PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_selectable (GTK_LABEL (value), TRUE);

    gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), value, 1, row, 1, 1);

    return value;
}

/* Show file metadata. On first call, creates the grid and stores
 * value label pointers. Subsequent calls just update the labels. */
static void
show_file_metadata (NemoPreviewPanel *panel)
{
    NemoFile *file = panel->priv->current_file;
    gchar *str;
    goffset size;

    if (G_UNLIKELY (panel->priv->metadata_grid == NULL)) {
        panel->priv->metadata_grid = gtk_grid_new ();
        gtk_grid_set_row_spacing (GTK_GRID (panel->priv->metadata_grid), 6);
        gtk_grid_set_column_spacing (GTK_GRID (panel->priv->metadata_grid), 8);
        gtk_container_set_border_width (GTK_CONTAINER (panel->priv->metadata_grid), 8);

        panel->priv->meta_type_val = add_metadata_row (panel,
            panel->priv->metadata_grid, _("Type:"), "", 0);
        panel->priv->meta_size_val = add_metadata_row (panel,
            panel->priv->metadata_grid, _("Size:"), "", 1);
        panel->priv->meta_location_val = add_metadata_row (panel,
            panel->priv->metadata_grid, _("Location:"), "", 2);
        panel->priv->meta_info_val = add_metadata_row (panel,
            panel->priv->metadata_grid, _("Info:"), "", 3);

        gtk_box_pack_start (GTK_BOX (panel), panel->priv->metadata_grid,
                            FALSE, FALSE, 2);
    }

    /* Update values in-place */
    str = nemo_file_get_mime_type (file);
    gtk_label_set_text (GTK_LABEL (panel->priv->meta_type_val),
                        str ? str : _("Unknown"));
    g_free (str);

    size = nemo_file_get_size (file);
    str = g_format_size (size);
    gtk_label_set_text (GTK_LABEL (panel->priv->meta_size_val), str);
    g_free (str);

    str = nemo_file_get_uri (file);
    gtk_label_set_text (GTK_LABEL (panel->priv->meta_location_val),
                        str ? str : "");
    g_free (str);

    str = nemo_file_get_description (file);
    gtk_label_set_text (GTK_LABEL (panel->priv->meta_info_val),
                        (str != NULL && str[0] != '\0') ? str : "");
    g_free (str);

    gtk_widget_show_all (panel->priv->metadata_grid);
}

/* ── Image preview with dynamic scaling ──────────────────────── */

static void
preview_image_size_allocate_cb (GtkWidget *widget,
                                GtkAllocation *allocation,
                                NemoPreviewPanel *panel)
{
    GdkPixbuf *scaled;
    gint avail_w;

    if (panel->priv->full_pixbuf == NULL)
        return;

    avail_w = allocation->width;
    if (avail_w < 4)
        return; /* too small, not realized yet */

    /* Avoid redundant rescaling for minor size changes */
    {
        gint diff = avail_w - panel->priv->last_scaled_width;
        if (diff > -3 && diff < 3)
            return;
    }

    scaled = scale_pixbuf_to_width (panel->priv->full_pixbuf, avail_w);

    if (scaled != NULL) {
        gtk_image_set_from_pixbuf (GTK_IMAGE (panel->priv->preview_image),
                                   scaled);
        g_object_unref (scaled);
        panel->priv->last_scaled_width = avail_w;
    }
}

/* Show image preview using the full-resolution pixbuf */
static void
show_image_preview (NemoPreviewPanel *panel)
{
    NemoFile *file = panel->priv->current_file;
    gchar *local_path = NULL;
    GFile *gfile;
    GError *error = NULL;

    /* Free old full pixbuf */
    g_clear_object (&panel->priv->full_pixbuf);
    panel->priv->last_scaled_width = 0;

    gfile = nemo_file_get_location (file);
    if (gfile == NULL)
        return;

    local_path = g_file_get_path (gfile);

    /* Check file size before loading to avoid OOM on huge images */
    {
        GFileInfo *info;

        info = g_file_query_info (gfile, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                  G_FILE_QUERY_INFO_NONE, NULL, NULL);
        if (info != NULL) {
            goffset file_size = g_file_info_get_size (info);

            g_object_unref (info);

            if (file_size > MAX_IMAGE_FILE_SIZE) {
                g_warning ("Image file too large for preview: %s "
                           "(%" G_GUINT64_FORMAT " bytes)",
                           local_path, (guint64) file_size);
                g_free (local_path);
                g_object_unref (gfile);
                return;
            }
        }
    }

    g_object_unref (gfile);

    if (local_path == NULL)
        return;

    /* Load the full resolution image */
    panel->priv->full_pixbuf = gdk_pixbuf_new_from_file (local_path, &error);

    if (panel->priv->full_pixbuf != NULL) {
        gint img_w, img_h;
        gint avail_w;

        img_w = gdk_pixbuf_get_width (panel->priv->full_pixbuf);
        img_h = gdk_pixbuf_get_height (panel->priv->full_pixbuf);

        gtk_widget_show (panel->priv->preview_image);
        gtk_widget_hide (panel->priv->text_scrolled);
        gtk_widget_hide (panel->priv->placeholder_label);

        /* Update dimensions label */
        {
            gchar *dim_text;
            dim_text = g_strdup_printf (_("%d × %d pixels"), img_w, img_h);
            gtk_label_set_text (GTK_LABEL (panel->priv->type_label), dim_text);
            g_free (dim_text);
        }

        /* Scale immediately to the panel's current width so the
         * GtkImage gets a proper pixbuf before the first allocation.
         * The size-allocate callback handles subsequent resizes. */
        avail_w = gtk_widget_get_allocated_width (GTK_WIDGET (panel));
        if (avail_w < 4) {
            avail_w = gtk_widget_get_allocated_width (
                gtk_widget_get_parent (GTK_WIDGET (panel)));
        }
        /* Fallback: if panel isn't laid out yet, use default width */
        if (avail_w < 4)
            avail_w = PREVIEW_DEFAULT_WIDTH;

        if (avail_w > 4) {
            GdkPixbuf *scaled;

            avail_w -= PREVIEW_INTERNAL_PADDING;
            if (avail_w < 4)
                avail_w = 4;

            scaled = scale_pixbuf_to_width (panel->priv->full_pixbuf,
                                            avail_w);
            if (scaled != NULL) {
                gtk_image_set_from_pixbuf (
                    GTK_IMAGE (panel->priv->preview_image), scaled);
                g_object_unref (scaled);
                panel->priv->last_scaled_width = avail_w;
            }
        }

        /* Queue resize so the size-allocate callback fires for
         * future panel resizes. */
        gtk_widget_queue_resize (GTK_WIDGET (panel));
    } else {
        /* Failed to load — show large icon as fallback */
        GIcon *icon;
        GtkIconInfo *icon_info;
        GdkPixbuf *icon_pixbuf;

        icon = nemo_file_get_gicon (file, 0);
        if (icon != NULL) {
            icon_info = gtk_icon_theme_lookup_by_gicon (
                gtk_icon_theme_get_default (),
                icon, PREVIEW_ICON_LARGE_SIZE, 0);
            if (icon_info != NULL) {
                icon_pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
                if (icon_pixbuf != NULL) {
                    gtk_image_set_from_pixbuf (
                        GTK_IMAGE (panel->priv->preview_image),
                        icon_pixbuf);
                    g_object_unref (icon_pixbuf);
                }
                g_object_unref (icon_info);
            }
        }
        gtk_widget_show (panel->priv->preview_image);

        if (error != NULL) {
            g_warning ("Failed to load preview image: %s", error->message);
            g_error_free (error);
        }
    }

    g_free (local_path);
}

/* ── Text file preview ───────────────────────────────────────── */

static void
show_text_preview (NemoPreviewPanel *panel)
{
    NemoFile *file = panel->priv->current_file;
    gchar *local_path = NULL;
    GFile *gfile;
    gchar *contents = NULL;
    gsize length;
    GError *error = NULL;
    GtkTextBuffer *buffer;

    gfile = nemo_file_get_location (file);
    if (gfile == NULL)
        return;

    local_path = g_file_get_path (gfile);
    g_object_unref (gfile);

    if (local_path == NULL)
        return;

    if (!g_file_get_contents (local_path, &contents, &length, &error)) {
        g_warning ("Failed to read text file: %s", error->message);
        g_error_free (error);
        g_free (local_path);
        return;
    }

    /* Truncate if too large */
    if (length > MAX_TEXT_PREVIEW_BYTES) {
        contents[MAX_TEXT_PREVIEW_BYTES] = '\0';
        length = MAX_TEXT_PREVIEW_BYTES;
    }

    /* Replace null bytes in binary files */
    {
        gsize i;
        for (i = 0; i < length; i++) {
            if (contents[i] == '\0')
                contents[i] = ' ';
        }
    }

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (panel->priv->text_view));
    gtk_text_buffer_set_text (buffer, contents, length);

    /* Make read-only */
    gtk_text_view_set_editable (GTK_TEXT_VIEW (panel->priv->text_view), FALSE);

    gtk_widget_show (panel->priv->text_scrolled);
    gtk_widget_hide (panel->priv->preview_image);
    gtk_widget_hide (panel->priv->placeholder_label);

    g_free (contents);
    g_free (local_path);
}

/* ── Directory preview ───────────────────────────────────────── */

static void
show_directory_preview (NemoPreviewPanel *panel)
{
    GIcon *icon;
    GtkIconInfo *icon_info;
    GdkPixbuf *icon_pixbuf;

    /* Show large folder icon */
    icon = nemo_file_get_gicon (panel->priv->current_file, 0);
    if (icon != NULL) {
        icon_info = gtk_icon_theme_lookup_by_gicon (
            gtk_icon_theme_get_default (),
            icon, PREVIEW_ICON_LARGE_SIZE, 0);
        if (icon_info != NULL) {
            icon_pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
            if (icon_pixbuf != NULL) {
                gtk_image_set_from_pixbuf (
                    GTK_IMAGE (panel->priv->preview_image),
                    icon_pixbuf);
                g_object_unref (icon_pixbuf);
            }
            g_object_unref (icon_info);
        }
    }

    gtk_label_set_text (GTK_LABEL (panel->priv->type_label), _("Folder"));

    gtk_widget_show (panel->priv->preview_image);
    gtk_widget_hide (panel->priv->text_scrolled);
    gtk_widget_hide (panel->priv->placeholder_label);

    show_file_metadata (panel);
}

/* ── Content routing ─────────────────────────────────────────── */

/* Determine file type and show appropriate preview */
static void
update_preview_content (NemoPreviewPanel *panel)
{
    NemoFile *file = panel->priv->current_file;
    gchar *name;
    gchar *mime_type;
    GIcon *icon;
    GtkIconInfo *icon_info;
    GdkPixbuf *icon_pixbuf;

    if (file == NULL)
        return;

    name = nemo_file_get_display_name (file);
    mime_type = nemo_file_get_mime_type (file);

    /* Update header: icon + filename */
    icon = nemo_file_get_gicon (file, 0);
    if (icon != NULL) {
        icon_info = gtk_icon_theme_lookup_by_gicon (
            gtk_icon_theme_get_default (), icon, 32, 0);
        if (icon_info != NULL) {
            icon_pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
            if (icon_pixbuf != NULL) {
                gtk_image_set_from_pixbuf (
                    GTK_IMAGE (panel->priv->icon_image),
                    icon_pixbuf);
                g_object_unref (icon_pixbuf);
            }
            g_object_unref (icon_info);
        }
    }

    gtk_label_set_text (GTK_LABEL (panel->priv->name_label), name);
    gtk_label_set_text (GTK_LABEL (panel->priv->type_label),
                        mime_type ? mime_type : _("Unknown type"));

    /* Directory preview */
    if (nemo_file_is_directory (file)) {
        show_directory_preview (panel);
        goto out;
    }

    /* Decide which preview to show based on mime type */
    if (mime_type != NULL) {
        if (g_str_has_prefix (mime_type, "image/")) {
            show_image_preview (panel);
            show_file_metadata (panel);
            goto out;
        }

        if (g_str_has_prefix (mime_type, "text/") ||
            g_strcmp0 (mime_type, "application/json") == 0 ||
            g_strcmp0 (mime_type, "application/xml") == 0 ||
            g_strcmp0 (mime_type, "application/x-shellscript") == 0 ||
            g_str_has_suffix (mime_type, "xml") ||
            g_str_has_suffix (mime_type, "yaml")) {
            show_text_preview (panel);
            show_file_metadata (panel);
            goto out;
        }
    }

    /* Fallback: show large icon + metadata */
    {
        GIcon *l_icon;
        GtkIconInfo *l_info;
        GdkPixbuf *l_pixbuf;

        l_icon = nemo_file_get_gicon (file, 0);
        if (l_icon != NULL) {
            l_info = gtk_icon_theme_lookup_by_gicon (
                gtk_icon_theme_get_default (),
                l_icon, PREVIEW_ICON_LARGE_SIZE, 0);
            if (l_info != NULL) {
                l_pixbuf = gtk_icon_info_load_icon (l_info, NULL);
                if (l_pixbuf != NULL) {
                    gtk_image_set_from_pixbuf (
                        GTK_IMAGE (panel->priv->preview_image),
                        l_pixbuf);
                    g_object_unref (l_pixbuf);
                }
                g_object_unref (l_info);
            }
        }
        gtk_widget_show (panel->priv->preview_image);
        gtk_widget_hide (panel->priv->text_scrolled);
        gtk_label_set_text (GTK_LABEL (panel->priv->type_label),
                            mime_type ? mime_type : _("Unknown type"));
    }

    show_file_metadata (panel);

out:
    g_free (name);
    g_free (mime_type);
}

/* ── Placeholder (no file selected) ──────────────────────────── */

static void
show_placeholder (NemoPreviewPanel *panel)
{
    gtk_label_set_text (GTK_LABEL (panel->priv->placeholder_label),
                        _("Select a file to preview"));

    gtk_widget_hide (panel->priv->preview_image);
    gtk_widget_hide (panel->priv->text_scrolled);
    gtk_widget_show (panel->priv->placeholder_label);

    /* Clear header */
    gtk_image_clear (GTK_IMAGE (panel->priv->icon_image));
    gtk_label_set_text (GTK_LABEL (panel->priv->name_label),
                        _("No file selected"));
    gtk_label_set_text (GTK_LABEL (panel->priv->type_label), "");

    /* Hide metadata (don't destroy — it gets updated on next file) */
    if (panel->priv->metadata_grid != NULL)
        gtk_widget_hide (panel->priv->metadata_grid);
}

/* ── Public API ──────────────────────────────────────────────── */

GtkWidget *
nemo_preview_panel_new (void)
{
    return GTK_WIDGET (g_object_new (NEMO_TYPE_PREVIEW_PANEL, NULL));
}

void
nemo_preview_panel_set_file (NemoPreviewPanel *panel, NemoFile *file)
{
    g_return_if_fail (NEMO_IS_PREVIEW_PANEL (panel));

    /* Clear old file reference */
    if (panel->priv->current_file != NULL) {
        g_object_unref (panel->priv->current_file);
        panel->priv->current_file = NULL;
    }

    if (file == NULL) {
        show_placeholder (panel);
        return;
    }

    panel->priv->current_file = g_object_ref (file);
    update_preview_content (panel);
}

void
nemo_preview_panel_clear (NemoPreviewPanel *panel)
{
    g_return_if_fail (NEMO_IS_PREVIEW_PANEL (panel));

    g_clear_object (&panel->priv->full_pixbuf);
    panel->priv->last_scaled_width = 0;

    if (panel->priv->current_file != NULL) {
        g_object_unref (panel->priv->current_file);
        panel->priv->current_file = NULL;
    }

    show_placeholder (panel);
}

/* ── GObject boilerplate ─────────────────────────────────────── */

static void
nemo_preview_panel_class_init (NemoPreviewPanelClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->dispose = nemo_preview_panel_dispose;
}

static void
nemo_preview_panel_init (NemoPreviewPanel *panel)
{
    GtkWidget *header_box;
    GtkWidget *label_box;
    GtkStyleContext *context;

    panel->priv = nemo_preview_panel_get_instance_private (panel);

    gtk_orientable_set_orientation (GTK_ORIENTABLE (panel),
                                    GTK_ORIENTATION_VERTICAL);
    gtk_box_set_spacing (GTK_BOX (panel), 4);
    gtk_container_set_border_width (GTK_CONTAINER (panel), 8);

    context = gtk_widget_get_style_context (GTK_WIDGET (panel));
    gtk_style_context_add_class (context, "nemo-preview-panel");

    /* --- Header row: icon + filename + type --- */
    header_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_bottom (header_box, 8);
    gtk_box_pack_start (GTK_BOX (panel), header_box, FALSE, FALSE, 0);

    /* Icon */
    panel->priv->icon_image = gtk_image_new ();
    gtk_box_pack_start (GTK_BOX (header_box), panel->priv->icon_image,
                        FALSE, FALSE, 0);

    /* Name + type labels (vertical) */
    label_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start (GTK_BOX (header_box), label_box, TRUE, TRUE, 0);

    panel->priv->name_label = gtk_label_new (_("No file selected"));
    gtk_label_set_ellipsize (GTK_LABEL (panel->priv->name_label),
                             PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_selectable (GTK_LABEL (panel->priv->name_label), TRUE);
    gtk_widget_set_halign (panel->priv->name_label, GTK_ALIGN_START);
    gtk_box_pack_start (GTK_BOX (label_box), panel->priv->name_label,
                        TRUE, TRUE, 0);

    panel->priv->type_label = gtk_label_new ("");
    gtk_label_set_ellipsize (GTK_LABEL (panel->priv->type_label),
                             PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign (panel->priv->type_label, GTK_ALIGN_START);
    gtk_box_pack_start (GTK_BOX (label_box), panel->priv->type_label,
                        TRUE, TRUE, 0);

    /* Separator */
    panel->priv->separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start (GTK_BOX (panel), panel->priv->separator,
                        FALSE, FALSE, 0);

    /* Preview image (for images, large icons) */
    panel->priv->preview_image = gtk_image_new ();
    gtk_widget_set_valign (panel->priv->preview_image, GTK_ALIGN_START);
    gtk_widget_set_halign (panel->priv->preview_image, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (panel), panel->priv->preview_image,
                        TRUE, TRUE, 0);

    /* Rescale full-resolution pixbuf when the image widget gets sized */
    g_signal_connect (panel->priv->preview_image, "size-allocate",
                      G_CALLBACK (preview_image_size_allocate_cb), panel);

    /* Scrolled text view (for text files) */
    panel->priv->text_scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (panel->priv->text_scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (panel), panel->priv->text_scrolled,
                        TRUE, TRUE, 0);

    panel->priv->text_view = gtk_text_view_new ();
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (panel->priv->text_view),
                                 GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (panel->priv->text_view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (panel->priv->text_view),
                                      FALSE);
    gtk_container_add (GTK_CONTAINER (panel->priv->text_scrolled),
                       panel->priv->text_view);

    /* Placeholder */
    panel->priv->placeholder_label = gtk_label_new (_("Select a file to preview"));
    gtk_widget_set_valign (panel->priv->placeholder_label, GTK_ALIGN_CENTER);
    gtk_widget_set_halign (panel->priv->placeholder_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (panel), panel->priv->placeholder_label,
                        TRUE, TRUE, 0);

    /* Metadata grid — created lazily on first show_file_metadata() */
    panel->priv->metadata_grid = NULL;
    panel->priv->meta_type_val = NULL;
    panel->priv->meta_size_val = NULL;
    panel->priv->meta_location_val = NULL;
    panel->priv->meta_info_val = NULL;
    panel->priv->current_file = NULL;

    /* Show the initial state */
    gtk_widget_show_all (GTK_WIDGET (panel));
    show_placeholder (panel);
}
