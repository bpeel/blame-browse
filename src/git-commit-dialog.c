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

#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbox.h>

#include "git-commit-dialog.h"

static void git_commit_dialog_dispose (GObject *object);

static void git_commit_dialog_set_property (GObject *object,
					    guint property_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void git_commit_dialog_get_property (GObject *object,
					    guint property_id,
					    GValue *value,
					    GParamSpec *pspec);

static void git_commit_dialog_update_label (GitCommitDialog *cdiag);

G_DEFINE_TYPE (GitCommitDialog, git_commit_dialog, GTK_TYPE_DIALOG);

#define GIT_COMMIT_DIALOG_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GIT_TYPE_COMMIT_DIALOG, \
				GitCommitDialogPrivate))

struct _GitCommitDialogPrivate
{
  GitCommit *commit;

  GtkWidget *label;
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

  g_type_class_add_private (klass, sizeof (GitCommitDialogPrivate));
}

static void
git_commit_dialog_init (GitCommitDialog *self)
{
  GitCommitDialogPrivate *priv;

  priv = self->priv = GIT_COMMIT_DIALOG_GET_PRIVATE (self);

  priv->label = g_object_ref_sink (gtk_label_new (NULL));
  gtk_widget_show (priv->label);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (self)->vbox), priv->label,
		      TRUE, TRUE, 0);

  git_commit_dialog_update_label (self);
}

static void
git_commit_dialog_unref_commit (GitCommitDialog *cdiag)
{
  GitCommitDialogPrivate *priv = cdiag->priv;

  if (priv->commit)
    {
      g_object_unref (priv->commit);
      priv->commit = NULL;
    }
}

static void
git_commit_dialog_dispose (GObject *object)
{
  GitCommitDialog *self = (GitCommitDialog *) object;
  GitCommitDialogPrivate *priv = self->priv;

  git_commit_dialog_unref_commit (self);

  if (priv->label)
    {
      g_object_unref (priv->label);
      priv->label = NULL;
    }

  G_OBJECT_CLASS (git_commit_dialog_parent_class)->dispose (object);
}

GtkWidget *
git_commit_dialog_new (void)
{
  GtkWidget *self = g_object_new (GIT_TYPE_COMMIT_DIALOG, NULL);

  return self;
}

void
git_commit_dialog_set_commit (GitCommitDialog *cdiag,
			      GitCommit *commit)
{
  GitCommitDialogPrivate *priv;

  g_return_if_fail (GIT_IS_COMMIT_DIALOG (cdiag));
  g_return_if_fail (commit == NULL || GIT_IS_COMMIT (commit));

  priv = cdiag->priv;

  if (commit)
    g_object_ref (commit);

  git_commit_dialog_unref_commit (cdiag);

  priv->commit = commit;

  git_commit_dialog_update_label (cdiag);

  g_object_notify (G_OBJECT (cdiag), "commit");
}

GitCommit *
git_commit_dialog_get_commit (GitCommitDialog *cdiag)
{
  g_return_val_if_fail (GIT_IS_COMMIT_DIALOG (cdiag), NULL);

  return cdiag->priv->commit;
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
    }
}

static void
git_commit_dialog_update_label (GitCommitDialog *cdiag)
{
  GitCommitDialogPrivate *priv = cdiag->priv;

  if (priv->label)
    {
      if (priv->commit)
	gtk_label_set_text (GTK_LABEL (priv->label),
			    git_commit_get_hash (priv->commit));
      else
	gtk_label_set_text (GTK_LABEL (priv->label), "No commit");
    }
}
