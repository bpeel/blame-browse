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

#include "git-source-view.h"

#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "git-hash-view.h"
#include "git-annotated-source.h"
#include "git-marshal.h"
#include "git-common.h"
#include "git-enum-types.h"

static void git_source_view_dispose (GObject *object);

static void git_source_view_on_commit_selected (GitHashView *source,
                                                GitCommit *commit,
                                                GitSourceView *sview);

typedef struct
{
  GitAnnotatedSource *paint_source, *load_source;
  guint loading_completed_handler;
  guint commit_selected_handler;
  guint pulse_timeout;

  GtkWidget *source_box, *scrolled_win, *text_view, *hash_view;
  GtkWidget *error_box, *error_label;
  GtkWidget *progress_bar;
} GitSourceViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GitSourceView,
                            git_source_view,
                            GTK_TYPE_WIDGET);

enum
  {
    COMMIT_SELECTED,

    LAST_SIGNAL
  };

static guint client_signals[LAST_SIGNAL];

static void
git_source_view_class_init (GitSourceViewClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = git_source_view_dispose;

  client_signals[COMMIT_SELECTED]
    = g_signal_new ("commit-selected",
                    G_OBJECT_CLASS_TYPE (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GitSourceViewClass, commit_selected),
                    NULL, NULL,
                    _git_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1,
                    GIT_TYPE_COMMIT);

  const char *resource_name
    = "/uk/co/busydoingnothing/blamebrowse/source-view.ui";

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                               resource_name);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitSourceView,
                                                source_box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitSourceView,
                                                scrolled_win);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitSourceView,
                                                hash_view);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitSourceView,
                                                text_view);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitSourceView,
                                                error_box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitSourceView,
                                                error_label);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitSourceView,
                                                progress_bar);

  gtk_widget_class_set_layout_manager_type ((GtkWidgetClass *) (klass),
                                            GTK_TYPE_BOX_LAYOUT);
}

static void
git_source_view_init (GitSourceView *sview)
{
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  g_type_ensure (GIT_TYPE_HASH_VIEW);

  gtk_widget_init_template (GTK_WIDGET (sview));

  priv->commit_selected_handler
    = g_signal_connect (priv->hash_view, "commit-selected",
                        G_CALLBACK (git_source_view_on_commit_selected), sview);

  GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (sview));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (layout),
                                  GTK_ORIENTATION_VERTICAL);
}

static void
git_source_view_unref_loading_source (GitSourceView *sview)
{
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  if (priv->load_source)
    {
      g_signal_handler_disconnect (priv->load_source,
                                   priv->loading_completed_handler);
      g_object_unref (priv->load_source);
      priv->load_source = NULL;
    }
}

static void
remove_pulse_timeout (GitSourceView *sview)
{
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  if (priv->pulse_timeout)
    {
      g_source_remove (priv->pulse_timeout);
      priv->pulse_timeout = 0;
    }
}

static void
git_source_view_dispose (GObject *object)
{
  GitSourceView *sview = (GitSourceView *) object;
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  if (priv->paint_source)
    {
      g_object_unref (priv->paint_source);
      priv->paint_source = NULL;
    }

  git_source_view_unref_loading_source (sview);

  if (priv->hash_view)
    {
      g_signal_handler_disconnect (priv->hash_view,
                                   priv->commit_selected_handler);
    }

  remove_pulse_timeout (sview);

  gtk_widget_dispose_template (GTK_WIDGET (sview), GIT_TYPE_SOURCE_VIEW);

  G_OBJECT_CLASS (git_source_view_parent_class)->dispose (object);
}

GtkWidget *
git_source_view_new (void)
{
  GtkWidget *widget = g_object_new (GIT_TYPE_SOURCE_VIEW,
                                    NULL);

  return widget;
}

static void
copy_string_to_buffer (GtkTextBuffer *buffer, const char *s)
{
  while (*s)
    {
      const char *invalid;
      gboolean is_valid = g_utf8_validate (s, -1, &invalid);

      GtkTextIter iter;
      gtk_text_buffer_get_end_iter (buffer, &iter);
      gtk_text_buffer_insert (buffer, &iter, s, invalid - s);

      if (is_valid)
        break;

      /* append U+FFFD REPLACEMENT CHARACTER */
      gtk_text_buffer_get_end_iter (buffer, &iter);
      gtk_text_buffer_insert (buffer, &iter, "\357\277\275", -1);

      s = invalid + 1;
    }
}

static void
copy_source_to_text_view (GtkTextView *text_view,
                          GitAnnotatedSource *source)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (text_view);

  if (buffer == NULL)
    return;

  gtk_text_buffer_set_text (buffer, "", 0);

  gsize n_lines = git_annotated_source_get_n_lines (source);

  for (gsize i = 0; i < n_lines; i++)
    {
      const GitAnnotatedSourceLine *line =
        git_annotated_source_get_line (source, i);

      copy_string_to_buffer (buffer, line->text);
    }
}

static void
git_source_view_on_commit_selected (GitHashView *source,
                                    GitCommit *commit,
                                    GitSourceView *sview)
{
  g_signal_emit (sview,
                 client_signals[COMMIT_SELECTED],
                 0,
                 commit);
}

static void
hide_progress_bar (GitSourceView *sview)
{
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  if (priv->progress_bar)
    {
      remove_pulse_timeout (sview);
      gtk_widget_set_visible (priv->progress_bar, FALSE);
    }
}

static void
set_error_state (GitSourceView *sview, const GError *error)
{
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  if (priv->error_box)
    gtk_widget_set_visible (priv->error_box, TRUE);

  if (priv->source_box)
    gtk_widget_set_visible (priv->source_box, FALSE);

  if (priv->error_label)
    gtk_label_set_text (GTK_LABEL (priv->error_label), error->message);
}

static void
git_source_view_on_completed (GitAnnotatedSource *source,
                              const GError *error,
                              GitSourceView *sview)
{
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  hide_progress_bar (sview);

  if (error)
    set_error_state (sview, error);
  else
    {
      /* Forget the old painting source */
      if (priv->paint_source)
        g_object_unref (priv->paint_source);
      /* Use the loading source to paint with */
      priv->paint_source = g_object_ref (source);

      if (priv->text_view)
        copy_source_to_text_view (GTK_TEXT_VIEW (priv->text_view), source);

      if (priv->hash_view)
        git_hash_view_set_source (GIT_HASH_VIEW (priv->hash_view), source);

      if (priv->error_box)
        gtk_widget_set_visible (priv->error_box, FALSE);

      if (priv->source_box)
        gtk_widget_set_visible (priv->source_box, TRUE);
    }

  git_source_view_unref_loading_source (sview);
}

static gboolean
pulse_cb (gpointer user_data)
{
  GitSourceView *sview = user_data;
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  if (priv->progress_bar)
    {
      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (priv->progress_bar));
      return G_SOURCE_CONTINUE;
    }
  else
    {
      priv->pulse_timeout = 0;
      return G_SOURCE_REMOVE;
    }
}

void
git_source_view_set_file (GitSourceView *sview,
                          GFile *file,
                          const gchar *revision)
{
  GError *error = NULL;

  g_return_if_fail (GIT_IS_SOURCE_VIEW (sview));
  g_return_if_fail (file != NULL);

  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  /* If we're currently trying to load some source then cancel it */
  git_source_view_unref_loading_source (sview);

  priv->load_source = git_annotated_source_new ();
  priv->loading_completed_handler
    = g_signal_connect (priv->load_source, "completed",
                        G_CALLBACK (git_source_view_on_completed), sview);

  if (git_annotated_source_fetch (priv->load_source,
                                  file, revision,
                                  &error))
    {
      if (priv->progress_bar)
        {
          gtk_widget_set_visible (priv->progress_bar, TRUE);
          gtk_progress_bar_pulse (GTK_PROGRESS_BAR (priv->progress_bar));

          if (priv->pulse_timeout == 0)
            priv->pulse_timeout = g_timeout_add (100, pulse_cb, sview);
        }
    }
  else
    {
      set_error_state (sview, error);
      git_source_view_unref_loading_source (sview);

      hide_progress_bar (sview);

      g_error_free (error);
    }
}
