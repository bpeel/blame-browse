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

#include "git-commit-dialog.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "git-commit-link-button.h"
#include "git-marshal.h"

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

static void git_commit_dialog_on_view_blame (GtkButton *button,
                                             gpointer user_data);

static gboolean git_commit_dialog_on_escape (GtkWidget *widget,
                                             GVariant *args,
                                             gpointer user_data);

typedef struct _GitCommitDialogButtonData GitCommitDialogButtonData;

typedef struct
{
  GitCommit *commit;
  guint has_log_data_handler;
  GitCommitDialogButtonData *buttons;

  GtkWidget *grid, *commit_label, *copy_button, *log_view;
  GtkWidget *view_blame_button, *close_button;

  guint view_blame_handler;
  guint close_handler;

  guint copy_handler;
} GitCommitDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GitCommitDialog,
                            git_commit_dialog,
                            GTK_TYPE_WINDOW);

struct _GitCommitDialogButtonData
{
  GtkWidget *label, *button;
  guint handler;

  GitCommitDialogButtonData *next;
};

enum
  {
    VIEW_BLAME,

    LAST_SIGNAL
  };

enum
  {
    PROP_0,

    PROP_COMMIT
  };

static guint client_signals[LAST_SIGNAL];

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

  client_signals[VIEW_BLAME]
    = g_signal_new ("view-blame",
                    G_OBJECT_CLASS_TYPE (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GitCommitDialogClass, view_blame),
                    NULL, NULL,
                    _git_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1,
                    GIT_TYPE_COMMIT);

  gtk_widget_class_add_binding (GTK_WIDGET_CLASS (klass),
                                GDK_KEY_Escape,
                                0, /* mods */
                                git_commit_dialog_on_escape,
                                NULL /* format_string */);

  const char *resource_name = "/uk/co/busydoingnothing/blamebrowse/"
    "commit-dialog.ui";

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                               resource_name);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitCommitDialog,
                                                grid);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitCommitDialog,
                                                commit_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitCommitDialog,
                                                copy_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitCommitDialog,
                                                log_view);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitCommitDialog,
                                                view_blame_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitCommitDialog,
                                                close_button);
}

static void
git_commit_dialog_init (GitCommitDialog *self)
{
  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  priv->copy_handler =
    g_signal_connect_swapped (priv->copy_button,
                              "clicked",
                              G_CALLBACK (git_commit_dialog_copy_hash),
                              self);

  priv->view_blame_handler =
    g_signal_connect (priv->view_blame_button,
                      "clicked",
                      G_CALLBACK (git_commit_dialog_on_view_blame),
                      self);

  priv->close_handler =
    g_signal_connect_swapped (priv->close_button,
                              "clicked",
                              G_CALLBACK (gtk_window_close),
                              self);
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

      if (priv->grid)
        {
          gtk_grid_remove (GTK_GRID (priv->grid), bdata->label);
          gtk_grid_remove (GTK_GRID (priv->grid), bdata->button);
        }

      g_object_unref (bdata->label);

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
  git_commit_dialog_unref_buttons (self);

  if (priv->copy_button)
    g_signal_handler_disconnect (priv->copy_button, priv->copy_handler);

  if (priv->view_blame_button)
    {
      g_signal_handler_disconnect (priv->view_blame_button,
                                   priv->view_blame_handler);
    }

  if (priv->close_button)
    g_signal_handler_disconnect (priv->close_button, priv->close_handler);

  gtk_widget_dispose_template (GTK_WIDGET (self), GIT_TYPE_COMMIT_DIALOG);

  G_OBJECT_CLASS (git_commit_dialog_parent_class)->dispose (object);
}

GtkWidget *
git_commit_dialog_new (GtkWindow *parent)
{
  GtkWidget *self = g_object_new (GIT_TYPE_COMMIT_DIALOG, NULL);

  if (parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (self), parent);
      gtk_window_set_destroy_with_parent (GTK_WINDOW (self), TRUE);
    }

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
git_commit_dialog_add_commit_button (GitCommitDialog *cdiag,
                                     GtkWidget *label,
                                     GtkWidget *button)
{
  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (cdiag);
  GitCommitDialogButtonData *bdata = g_slice_new (GitCommitDialogButtonData);

  bdata->label = g_object_ref_sink (label);

  bdata->button = g_object_ref_sink (button);
  bdata->handler
    = g_signal_connect (button, "clicked",
                        G_CALLBACK (git_commit_dialog_on_commit_selected),
                        cdiag);

  bdata->next = priv->buttons;
  priv->buttons = bdata;
}

static void
git_commit_dialog_update (GitCommitDialog *cdiag)
{
  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (cdiag);

  git_commit_dialog_unref_buttons (cdiag);

  if (priv->commit_label)
    {
      gtk_label_set_text (GTK_LABEL (priv->commit_label),
                          priv->commit
                          ? git_commit_get_hash (priv->commit)
                          : "");
    }

  if (priv->grid && priv->commit && git_commit_get_has_log_data (priv->commit))
    {
      int y = 1;

      for (const GSList *node = git_commit_get_parents (priv->commit);
           node;
           node = node->next, y++)
        {
          GitCommit *commit = (GitCommit *) node->data;

          GtkWidget *label = gtk_label_new (NULL);
          g_object_set (label, "use-markup", TRUE,
                        "label", _("<b>Parent</b>"),
                        NULL);
          gtk_grid_attach (GTK_GRID (priv->grid),
                           label,
                           0, y, 1, 1);

          GtkWidget *button = git_commit_link_button_new (commit);
          gtk_grid_attach (GTK_GRID (priv->grid),
                           button,
                           1, y, 2, 1);

          git_commit_dialog_add_commit_button (cdiag, label, button);
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
      GdkClipboard *clipboard = gdk_display_get_clipboard(display);

      if (clipboard)
        {
          gdk_clipboard_set_text (clipboard,
                                  git_commit_get_hash (priv->commit));
        }
    }
}

static void
git_commit_dialog_on_view_blame (GtkButton *button,
                                 gpointer user_data)
{
  GitCommitDialog *cdiag = user_data;
  GitCommitDialogPrivate *priv = git_commit_dialog_get_instance_private (cdiag);

  if (priv->commit)
    g_signal_emit (cdiag, client_signals[VIEW_BLAME], 0, priv->commit);
}

static gboolean
git_commit_dialog_on_escape (GtkWidget *widget,
                             GVariant *args,
                             gpointer user_data)
{
  GitCommitDialog *cdiag = GIT_COMMIT_DIALOG (widget);

  gtk_window_close (GTK_WINDOW (cdiag));

  return TRUE;
}
