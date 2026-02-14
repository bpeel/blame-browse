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

#include "git-commit-link-button.h"
#include "git-commit.h"

static void git_commit_link_button_dispose (GObject *object);
static void git_commit_link_button_set_property (GObject *object,
                                                 guint property_id,
                                                 const GValue *value,
                                                 GParamSpec *pspec);
static void git_commit_link_button_get_property (GObject *object,
                                                 guint property_id,
                                                 GValue *value,
                                                 GParamSpec *pspec);
static void git_commit_link_button_clicked (GtkButton *button);

struct _GitCommitLinkButton
{
  GtkLinkButton parent;
};

typedef struct
{
  GitCommit *commit;
} GitCommitLinkButtonPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (GitCommitLinkButton,
                                  git_commit_link_button,
                                  GTK_TYPE_LINK_BUTTON);

enum
  {
    PROP_0,

    PROP_COMMIT
  };

static void
git_commit_link_button_class_init (GitCommitLinkButtonClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkButtonClass *button_class = (GtkButtonClass *) klass;
  GParamSpec *pspec;

  gobject_class->dispose = git_commit_link_button_dispose;
  gobject_class->set_property = git_commit_link_button_set_property;
  gobject_class->get_property = git_commit_link_button_get_property;

  button_class->clicked = git_commit_link_button_clicked;

  pspec = g_param_spec_object ("commit",
                               "Commit",
                               "The commit object to link to",
                               GIT_TYPE_COMMIT,
                               G_PARAM_READABLE | G_PARAM_WRITABLE);
  g_object_class_install_property (gobject_class, PROP_COMMIT, pspec);
}

static void
git_commit_link_button_init (GitCommitLinkButton *self)
{
}

static void
git_commit_link_button_unref_commit (GitCommitLinkButton *lbutton)
{
  GitCommitLinkButtonPrivate *priv =
    git_commit_link_button_get_instance_private (lbutton);

  if (priv->commit)
    {
      g_object_unref (priv->commit);
      priv->commit = NULL;
    }
}

static void
git_commit_link_button_dispose (GObject *object)
{
  GitCommitLinkButton *self = (GitCommitLinkButton *) object;

  git_commit_link_button_unref_commit (self);

  G_OBJECT_CLASS (git_commit_link_button_parent_class)->dispose (object);
}

static void
git_commit_link_button_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
  GitCommitLinkButton *lbutton = (GitCommitLinkButton *) object;

  switch (property_id)
    {
    case PROP_COMMIT:
      git_commit_link_button_set_commit (lbutton, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
git_commit_link_button_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
  GitCommitLinkButton *lbutton = (GitCommitLinkButton *) object;

  switch (property_id)
    {
    case PROP_COMMIT:
      g_value_set_object (value, git_commit_link_button_get_commit (lbutton));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
git_commit_link_button_clicked (GtkButton *button)
{
  /* Override the default clicked handler so that it won't try to open
     the commit hash as a URL */
}

void
git_commit_link_button_set_commit (GitCommitLinkButton *lbutton,
                                   GitCommit *commit)
{
  GitCommitLinkButtonPrivate *priv =
    git_commit_link_button_get_instance_private (lbutton);

  if (commit)
    g_object_ref (commit);

  git_commit_link_button_unref_commit (lbutton);

  priv->commit = commit;

  if (commit)
    gtk_button_set_label (GTK_BUTTON (lbutton),
                          git_commit_get_hash (commit));
  else
    gtk_button_set_label (GTK_BUTTON (lbutton), "");

  g_object_notify (G_OBJECT (lbutton), "commit");
}

GitCommit *
git_commit_link_button_get_commit (GitCommitLinkButton *lbutton)
{
  GitCommitLinkButtonPrivate *priv =
    git_commit_link_button_get_instance_private (lbutton);

  return priv->commit;
}

GtkWidget *
git_commit_link_button_new (GitCommit *commit)
{
  GtkWidget *self = g_object_new (GIT_TYPE_COMMIT_LINK_BUTTON,
                                  "commit", commit,
                                  NULL);

  return self;
}
