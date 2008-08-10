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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkscrolledwindow.h>

#include "git-main-window.h"
#include "git-source-view.h"

static void git_main_window_dispose (GObject *object);

G_DEFINE_TYPE (GitMainWindow, git_main_window, GTK_TYPE_WINDOW);

#define GIT_MAIN_WINDOW_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GIT_TYPE_MAIN_WINDOW, \
				GitMainWindowPrivate))

struct _GitMainWindowPrivate
{
  GtkWidget *statusbar, *source_view;
};

static void
git_main_window_class_init (GitMainWindowClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = git_main_window_dispose;

  g_type_class_add_private (klass, sizeof (GitMainWindowPrivate));
}

static void
git_main_window_init (GitMainWindow *self)
{
  GitMainWindowPrivate *priv;
  GtkWidget *layout, *scrolled_win;

  priv = self->priv = GIT_MAIN_WINDOW_GET_PRIVATE (self);

  layout = gtk_vbox_new (FALSE, 0);

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);

  priv->source_view = g_object_ref_sink (git_source_view_new ());
  gtk_widget_show (priv->source_view);
  gtk_container_add (GTK_CONTAINER (scrolled_win), priv->source_view);
  
  gtk_widget_show (scrolled_win);
  gtk_box_pack_start (GTK_BOX (layout), scrolled_win, TRUE, TRUE, 0);

  priv->statusbar = g_object_ref_sink (gtk_statusbar_new ());
  gtk_widget_show (priv->statusbar);
  gtk_box_pack_start (GTK_BOX (layout), priv->statusbar, FALSE, FALSE, 0);

  gtk_widget_show (layout);
  gtk_container_add (GTK_CONTAINER (self), layout);
}

static void
git_main_window_dispose (GObject *object)
{
  GitMainWindow *self = (GitMainWindow *) object;
  GitMainWindowPrivate *priv = self->priv;

  if (priv->statusbar)
    {
      g_object_unref (priv->statusbar);
      priv->statusbar = NULL;
    }

  if (priv->source_view)
    {
      g_object_unref (priv->source_view);
      priv->source_view = NULL;
    }

  G_OBJECT_CLASS (git_main_window_parent_class)->dispose (object);
}

GtkWidget *
git_main_window_new (void)
{
  GtkWidget *self = g_object_new (GIT_TYPE_MAIN_WINDOW,
				  "type", GTK_WINDOW_TOPLEVEL,
				  NULL);

  return self;
}

void
git_main_window_set_file (GitMainWindow *main_window,
			  const gchar *filename,
			  const gchar *revision)
{
  GitMainWindowPrivate *priv;

  g_return_if_fail (GIT_IS_MAIN_WINDOW (main_window));
  g_return_if_fail (filename != NULL);

  priv = main_window->priv;

  if (priv->source_view)
    git_source_view_set_file (GIT_SOURCE_VIEW (priv->source_view),
			      filename, revision);
}
