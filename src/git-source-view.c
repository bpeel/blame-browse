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
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "git-source-view.h"
#include "git-hash-view.h"
#include "git-annotated-source.h"
#include "git-marshal.h"
#include "git-common.h"
#include "git-enum-types.h"

static void git_source_view_dispose (GObject *object);
static void git_source_view_destroy (GtkWidget *widget);
static void git_source_view_get_property (GObject *object, guint property_id,
                                          GValue *value, GParamSpec *pspec);

static void git_source_view_on_commit_selected (GitHashView *source,
                                                GitCommit *commit,
                                                GitSourceView *sview);

typedef struct
{
  GitAnnotatedSource *paint_source, *load_source;
  guint loading_completed_handler;
  guint commit_selected_handler;

  GitSourceViewState state;
  GError *state_error;

  GtkWidget *text_view, *hash_view;
} GitSourceViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GitSourceView,
                            git_source_view,
                            GTK_TYPE_BOX);

enum
  {
    PROP_0,

    PROP_STATE,
  };

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
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
  GParamSpec *pspec;

  gobject_class->get_property = git_source_view_get_property;
  gobject_class->dispose = git_source_view_dispose;

  widget_class->destroy = git_source_view_destroy;

  pspec = g_param_spec_enum ("state",
                             "State",
                             "The state of loading the annotated source. "
                             "Either ready, loading or error.",
                             GIT_TYPE_SOURCE_VIEW_STATE,
                             GIT_SOURCE_VIEW_READY,
                             G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_STATE, pspec);

  client_signals[COMMIT_SELECTED]
    = g_signal_new ("commit-selected",
                    G_OBJECT_CLASS_TYPE (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GitSourceViewClass, commit_selected),
                    NULL, NULL,
                    _git_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1,
                    GIT_TYPE_COMMIT);
}

static void
git_source_view_init (GitSourceView *sview)
{
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  g_object_set (sview,
                "spacing", 3,
                "orientation", GTK_ORIENTATION_HORIZONTAL,
                NULL);
  priv->state = GIT_SOURCE_VIEW_READY;
  priv->state_error = NULL;

  priv->text_view = gtk_text_view_new ();
  g_object_ref_sink (priv->text_view);

  priv->hash_view = git_hash_view_new (GTK_TEXT_VIEW (priv->text_view));
  g_object_ref_sink (priv->hash_view);

  priv->commit_selected_handler
    = g_signal_connect (priv->hash_view, "commit-selected",
                        G_CALLBACK (git_source_view_on_commit_selected), sview);

  gtk_widget_show (priv->hash_view);
  gtk_box_pack_start (GTK_BOX (sview),
                      priv->hash_view,
                      FALSE, /* expand */
                      TRUE, /* fill */
                      0 /* padding */);

  gtk_text_view_set_monospace (GTK_TEXT_VIEW (priv->text_view), TRUE);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (priv->text_view), FALSE);

  gtk_widget_show (priv->text_view);

  GtkWidget *scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  gtk_container_add (GTK_CONTAINER (scrolled_win), priv->text_view);

  gtk_widget_show (scrolled_win);

  gtk_box_pack_start (GTK_BOX (sview),
                      scrolled_win,
                      TRUE, /* expand */
                      TRUE, /* fill */
                      0 /* padding */);
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
git_source_view_unref_children (GitSourceView *sview)
{
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  if (priv->text_view)
    {
      g_object_unref (priv->text_view);
      priv->text_view = NULL;
    }

  if (priv->hash_view)
    {
      g_signal_handler_disconnect (priv->hash_view,
                                   priv->commit_selected_handler);
      g_object_unref (priv->hash_view);
      priv->hash_view = NULL;
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

  if (priv->state_error)
    {
      g_error_free (priv->state_error);
      priv->state_error = NULL;
    }

  git_source_view_unref_children (sview);

  G_OBJECT_CLASS (git_source_view_parent_class)->dispose (object);
}

static void
git_source_view_destroy (GtkWidget *widget)
{
  GitSourceView *sview = (GitSourceView *) widget;

  git_source_view_unref_children (sview);

  GTK_WIDGET_CLASS (git_source_view_parent_class)->destroy (widget);
}

static void
git_source_view_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  GitSourceView *sview = (GitSourceView *) object;
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  switch (property_id)
    {
    case PROP_STATE:
      g_value_set_enum (value, priv->state);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

GtkWidget *
git_source_view_new (void)
{
  GtkWidget *widget = g_object_new (GIT_TYPE_SOURCE_VIEW,
                                    NULL);

  return widget;
}

static void
git_source_view_set_state (GitSourceView *sview,
                           GitSourceViewState state,
                           const GError *error)
{
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  priv->state = state;
  if (priv->state_error)
    g_error_free (priv->state_error);
  if (error)
    priv->state_error = g_error_copy (error);
  else
    priv->state_error = NULL;

  g_object_notify (G_OBJECT (sview), "state");
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
git_source_view_on_completed (GitAnnotatedSource *source,
                              const GError *error,
                              GitSourceView *sview)
{
  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  if (error)
    git_source_view_set_state (sview, GIT_SOURCE_VIEW_ERROR, error);
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

      git_source_view_set_state (sview, GIT_SOURCE_VIEW_READY, NULL);
    }

  git_source_view_unref_loading_source (sview);
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

  if (!git_annotated_source_fetch (priv->load_source,
                                   file, revision,
                                   &error))
    {
      git_source_view_set_state (sview, GIT_SOURCE_VIEW_ERROR, error);
      git_source_view_unref_loading_source (sview);

      g_error_free (error);
    }
  else
    git_source_view_set_state (sview, GIT_SOURCE_VIEW_LOADING, NULL);
}

GitSourceViewState
git_source_view_get_state (GitSourceView *sview)
{
  g_return_val_if_fail (GIT_IS_SOURCE_VIEW (sview), 0);

  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  return priv->state;
}

const GError *
git_source_view_get_state_error (GitSourceView *sview)
{
  g_return_val_if_fail (GIT_IS_SOURCE_VIEW (sview), 0);

  GitSourceViewPrivate *priv = git_source_view_get_instance_private (sview);

  return priv->state_error;
}
