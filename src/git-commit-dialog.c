/* This file is part of blame-browse.
 * Copyright (C) 2008  Neil Roberts  <bpeeluk@yahoo.co.uk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "git-commit-dialog.h"
#include "git-commit-link-button.h"

static void git_commit_dialog_dispose (GObject *object);

static void git_commit_dialog_set_property (GObject *object,
                                            guint property_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void git_commit_dialog_get_property (GObject *object,
                                            guint property_id,
                                            GValue *value,
                                            GParamSpec *pspec);

static void git_commit_dialog_update (GitCommitDialog *cdiag);

static void git_commit_dialog_unref_buttons (GitCommitDialog *cdiag);

static void git_commit_dialog_copy_hash (GitCommitDialog *cdiag);

typedef struct _GitCommitDialogButtonData GitCommitDialogButtonData;

struct _GitCommitDialog
{
  GtkDialog parent;
};

typedef struct
{
  GitCommit *commit;
  guint has_log_data_handler;
  GitCommitDialogButtonData *buttons;

  GtkWidget *grid, *log_view, *commit_label;

  guint copy_handler;
  GtkWidget *copy_button;
} GitCommitDialogPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (GitCommitDialog,
                                  git_commit_dialog,
                                  GTK_TYPE_DIALOG);

struct _GitCommitDialogButtonData
{
  GtkWidget *button;
  guint handler;

  GitCommitDialogButtonData *next;
};

enum
  {
    PROP_0,

    PROP_COMMIT
  };

static void
git_commit_dialog_class_init (GitCommitDialogClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GParamSpec *pspec;

  gobject_class->dispose = git_commit_dialog_dispose;
  gobject_class->set_property = git_commit_dialog_set_property;
  gobject_class->get_property = git_commit_dialog_get_property;

  pspec = g_param_spec_object ("commit",
                               "Commit",
                               "The commit object to display data for",
                               GIT_TYPE_COMMIT,
                               G_PARAM_READABLE | G_PARAM_WRITABLE);
  g_object_class_install_property (gobject_class, PROP_COMMIT, pspec);
}

static void
git_commit_dialog_init (GitCommitDialog *self)
{
  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (self);
  GtkWidget *content, *scrolled_window;

  gtk_container_set_border_width (GTK_CONTAINER (self), 5);

  priv->grid = g_object_ref_sink (gtk_grid_new ());
  gtk_grid_set_column_spacing (GTK_GRID (priv->grid), 5);

  /* Labels for the commit hash */
  GtkWidget *label = gtk_label_new (NULL);
  g_object_set (label, "use-markup", TRUE,
                "label", _("<b>Commit</b>"),
                "xalign", 0.0f,
                NULL);
  gtk_widget_show (label);
  gtk_grid_attach (GTK_GRID (priv->grid), label, 0, 0, 1, 1);

  priv->commit_label = g_object_ref_sink (gtk_label_new (NULL));
  gtk_widget_show (priv->commit_label);
  gtk_grid_attach (GTK_GRID (priv->grid),
                   priv->commit_label,
                   1, 0, 1, 1);

  priv->copy_button = gtk_button_new_from_icon_name ("edit-copy",
                                                     GTK_ICON_SIZE_BUTTON);
  g_object_ref_sink (priv->copy_button);
  priv->copy_handler =
    g_signal_connect_swapped (priv->copy_button,
                              "clicked",
                              G_CALLBACK (git_commit_dialog_copy_hash),
                              self);
  gtk_widget_show (priv->copy_button);
  gtk_grid_attach (GTK_GRID (priv->grid),
                   priv->copy_button,
                   2, 0, 1, 1);

  gtk_widget_show (priv->grid);

  content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 11);
  gtk_container_set_border_width (GTK_CONTAINER (content), 5);

  gtk_box_pack_start (GTK_BOX (content), priv->grid, FALSE, FALSE, 0);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                       GTK_SHADOW_ETCHED_IN);

  priv->log_view = g_object_ref_sink (gtk_text_view_new ());
  g_object_set (priv->log_view,
                "editable", FALSE,
                "cursor-visible", FALSE,
                "left-margin", 8,
                "right-margin", 8,
                NULL);
  gtk_widget_show (priv->log_view);
  gtk_container_add (GTK_CONTAINER (scrolled_window), priv->log_view);

  gtk_widget_show (scrolled_window);
  gtk_box_pack_start (GTK_BOX (content), scrolled_window, TRUE, TRUE, 0);

  gtk_widget_show (content);

  GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));

  gtk_box_pack_start (GTK_BOX (content_area), content,
                      TRUE, TRUE, 0);

  gtk_dialog_add_buttons (GTK_DIALOG (self),
                          _("View _blame"),
                          GIT_COMMIT_DIALOG_RESPONSE_VIEW_BLAME,
                          dgettext("gtk30", "_Close"), GTK_RESPONSE_CLOSE,
                          NULL);
}

static void
git_commit_dialog_unref_commit (GitCommitDialog *cdiag)
{
  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (cdiag);

  if (priv->commit)
    {
      g_signal_handler_disconnect (priv->commit,
                                   priv->has_log_data_handler);
      g_object_unref (priv->commit);
      priv->commit = NULL;
    }
}

static void
git_commit_dialog_unref_buttons (GitCommitDialog *cdiag)
{
  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (cdiag);
  GitCommitDialogButtonData *bdata, *next;

  for (bdata = priv->buttons; bdata; bdata = next)
    {
      next = bdata->next;

      g_signal_handler_disconnect (bdata->button, bdata->handler);
      g_object_unref (bdata->button);
      g_slice_free (GitCommitDialogButtonData, bdata);
    }

  priv->buttons = NULL;
}

static void
git_commit_dialog_dispose (GObject *object)
{
  GitCommitDialog *self = (GitCommitDialog *) object;
  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (self);

  git_commit_dialog_unref_commit (self);

  if (priv->grid)
    {
      g_object_unref (priv->grid);
      priv->grid = NULL;
    }

  if (priv->log_view)
    {
      g_object_unref (priv->log_view);
      priv->log_view = NULL;
    }

  if (priv->commit_label)
    {
      g_object_unref (priv->commit_label);
      priv->commit_label = NULL;
    }

  if (priv->copy_button)
    {
      g_signal_handler_disconnect (priv->copy_button, priv->copy_handler);
      g_object_unref (priv->copy_button);
      priv->copy_button = NULL;
    }

  git_commit_dialog_unref_buttons (self);

  G_OBJECT_CLASS (git_commit_dialog_parent_class)->dispose (object);
}

GtkWidget *
git_commit_dialog_new (void)
{
  GtkWidget *self = g_object_new (GIT_TYPE_COMMIT_DIALOG, NULL);

  return self;
}

static void
git_commit_dialog_on_commit_selected (GtkButton *button, GitCommitDialog *cdiag)
{
  GitCommit *commit
    = git_commit_link_button_get_commit (GIT_COMMIT_LINK_BUTTON (button));

  git_commit_dialog_set_commit (cdiag, commit);
}

static void
git_commit_dialog_add_commit_button (GitCommitDialog *cdiag, GtkWidget *widget)
{
  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (cdiag);
  GitCommitDialogButtonData *bdata = g_slice_new (GitCommitDialogButtonData);

  bdata->button = g_object_ref_sink (widget);
  bdata->handler
    = g_signal_connect (widget, "clicked",
                        G_CALLBACK (git_commit_dialog_on_commit_selected),
                        cdiag);

  bdata->next = priv->buttons;
  priv->buttons = bdata;
}

static void
remove_if_parent_commit (GtkWidget *widget,
                         gpointer data)
{
  GtkContainer *container = data;
  int top_attach;

  gtk_container_child_get (container, widget, "top-attach", &top_attach, NULL);

  if (top_attach > 0)
    gtk_widget_destroy (widget);
}

static void
git_commit_dialog_update (GitCommitDialog *cdiag)
{
  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (cdiag);
  GtkWidget *widget;

  git_commit_dialog_unref_buttons (cdiag);

  if (priv->commit_label)
    {
      gtk_label_set_text (GTK_LABEL (priv->commit_label),
                          priv->commit
                          ? git_commit_get_hash (priv->commit)
                          : "");
    }

  if (priv->grid)
    {
      /* Remove all existing parent commit labels */
      gtk_container_foreach (GTK_CONTAINER (priv->grid),
                             remove_if_parent_commit,
                             priv->grid);

      if (priv->commit && git_commit_get_has_log_data (priv->commit))
        {
          const GSList *node;
          int y = 1;

          for (node = git_commit_get_parents (priv->commit);
               node;
               node = node->next, y++)
            {
              GitCommit *commit = (GitCommit *) node->data;

              widget = gtk_label_new (NULL);
              g_object_set (widget, "use-markup", TRUE,
                            "label", _("<b>Parent</b>"),
                            "xalign", 0.0f,
                            NULL);
              gtk_widget_show (widget);
              gtk_grid_attach (GTK_GRID (priv->grid),
                               widget,
                               0, y, 1, 1);

              widget = git_commit_link_button_new (commit);
              git_commit_dialog_add_commit_button (cdiag, widget);
              gtk_widget_show (widget);
              gtk_grid_attach (GTK_GRID (priv->grid),
                               widget,
                               1, y, 2, 1);
            }
        }
    }

  if (priv->log_view)
    {
      GtkTextBuffer *buffer
        = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->log_view));

      gtk_text_buffer_set_text (buffer,
                                priv->commit == NULL ? ""
                                : git_commit_get_has_log_data (priv->commit)
                                ? git_commit_get_log_data (priv->commit)
                                : _("Loading..."),
                                -1);
    }
}

void
git_commit_dialog_set_commit (GitCommitDialog *cdiag,
                              GitCommit *commit)
{
  g_return_if_fail (GIT_IS_COMMIT_DIALOG (cdiag));
  g_return_if_fail (commit == NULL || GIT_IS_COMMIT (commit));

  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (cdiag);

  if (commit)
    g_object_ref (commit);

  git_commit_dialog_unref_commit (cdiag);

  priv->commit = commit;

  if (commit)
    {
      priv->has_log_data_handler
        = g_signal_connect_swapped (commit, "notify::has-log-data",
                                    G_CALLBACK (git_commit_dialog_update),
                                    cdiag);
      git_commit_fetch_log_data (commit);
    }

  git_commit_dialog_update (cdiag);

  g_object_notify (G_OBJECT (cdiag), "commit");
}

GitCommit *
git_commit_dialog_get_commit (GitCommitDialog *cdiag)
{
  g_return_val_if_fail (GIT_IS_COMMIT_DIALOG (cdiag), NULL);

  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (cdiag);

  return priv->commit;
}

static void
git_commit_dialog_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  GitCommitDialog *cdiag = (GitCommitDialog *) object;

  switch (property_id)
    {
    case PROP_COMMIT:
      git_commit_dialog_set_commit (cdiag, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
git_commit_dialog_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  GitCommitDialog *cdiag = (GitCommitDialog *) object;

  switch (property_id)
    {
    case PROP_COMMIT:
      g_value_set_object (value, git_commit_dialog_get_commit (cdiag));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
git_commit_dialog_copy_hash (GitCommitDialog *cdiag)
{
  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (cdiag);

  if (priv->commit)
    {
      GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (cdiag));
      GtkClipboard *clipboard =
        gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);

      if (clipboard)
        {
          gtk_clipboard_set_text (clipboard,
                                  git_commit_get_hash (priv->commit),
                                  -1);
        }
    }
}
