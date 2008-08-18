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
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkstock.h>

#include "git-commit-dialog.h"
#include "git-commit-link-button.h"
#include "intl.h"

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

typedef struct _GitCommitDialogButtonData GitCommitDialogButtonData;

G_DEFINE_TYPE (GitCommitDialog, git_commit_dialog, GTK_TYPE_DIALOG);

#define GIT_COMMIT_DIALOG_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GIT_TYPE_COMMIT_DIALOG, \
				GitCommitDialogPrivate))

struct _GitCommitDialogPrivate
{
  GitCommit *commit;
  guint has_log_data_handler;
  GitCommitDialogButtonData *buttons;

  GtkWidget *table, *log_view;
};

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

  g_type_class_add_private (klass, sizeof (GitCommitDialogPrivate));
}

static void
git_commit_dialog_init (GitCommitDialog *self)
{
  GitCommitDialogPrivate *priv;
  GtkWidget *content, *scrolled_window;

  priv = self->priv = GIT_COMMIT_DIALOG_GET_PRIVATE (self);

  gtk_dialog_set_has_separator (GTK_DIALOG (self), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (self), 5);

  priv->table = g_object_ref_sink (gtk_table_new (0, 2, FALSE));
  gtk_table_set_col_spacing (GTK_TABLE (priv->table), 0, 5);
  gtk_widget_show (priv->table);

  g_object_set (GTK_DIALOG (self)->vbox, "spacing", 2, NULL);

  content = gtk_vbox_new (FALSE, 11);
  gtk_container_set_border_width (GTK_CONTAINER (content), 5);

  gtk_box_pack_start (GTK_BOX (content), priv->table, FALSE, FALSE, 0);

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
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (self)->vbox), content,
		      TRUE, TRUE, 0);

  gtk_dialog_add_buttons (GTK_DIALOG (self),
			  _("View _blame"),
			  GIT_COMMIT_DIALOG_RESPONSE_VIEW_BLAME,
			  GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			  NULL);
}

static void
git_commit_dialog_unref_commit (GitCommitDialog *cdiag)
{
  GitCommitDialogPrivate *priv = cdiag->priv;

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
  GitCommitDialogPrivate *priv = cdiag->priv;
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
  GitCommitDialogPrivate *priv = self->priv;

  git_commit_dialog_unref_commit (self);

  if (priv->table)
    {
      g_object_unref (priv->table);
      priv->table = NULL;
    }

  if (priv->log_view)
    {
      g_object_unref (priv->log_view);
      priv->log_view = NULL;
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
  GitCommitDialogPrivate *priv = cdiag->priv;
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
git_commit_dialog_update (GitCommitDialog *cdiag)
{
  GitCommitDialogPrivate *priv = cdiag->priv;
  GtkWidget *widget;

  git_commit_dialog_unref_buttons (cdiag);

  if (priv->table)
    {
      /* Remove all existing parent commit labels */
      gtk_container_foreach (GTK_CONTAINER (priv->table),
			     (GtkCallback) gtk_widget_destroy, NULL);

      if (priv->commit)
	{
	  /* Add a row for the commit's own hash */
	  widget = gtk_label_new (NULL);
	  g_object_set (widget, "use-markup", TRUE,
			"label", _("<b>Commit</b>"),
			"xalign", 0.0f,
			NULL);
	  gtk_widget_show (widget);
	  gtk_table_attach (GTK_TABLE (priv->table),
			    widget,
			    0, 1, 0, 1,
			    GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK,
			    0, 0);

	  widget = git_commit_link_button_new (priv->commit);
	  git_commit_dialog_add_commit_button (cdiag, widget);
	  gtk_widget_show (widget);
	  gtk_table_attach (GTK_TABLE (priv->table),
			    widget,
			    1, 2, 0, 1,
			    GTK_FILL | GTK_SHRINK | GTK_EXPAND,
			    GTK_FILL | GTK_SHRINK,
			    0, 0);

	  if (git_commit_get_has_log_data (priv->commit))
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
		  gtk_table_attach (GTK_TABLE (priv->table),
				    widget,
				    0, 1, y, y + 1,
				    GTK_FILL | GTK_SHRINK,
				    GTK_FILL | GTK_SHRINK,
				    0, 0);

		  widget = git_commit_link_button_new (commit);
		  git_commit_dialog_add_commit_button (cdiag, widget);
		  gtk_widget_show (widget);
		  gtk_table_attach (GTK_TABLE (priv->table),
				    widget,
				    1, 2, y, y + 1,
				    GTK_FILL | GTK_SHRINK | GTK_EXPAND,
				    GTK_FILL | GTK_SHRINK,
				    0, 0);
		}
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
  GitCommitDialogPrivate *priv;

  g_return_if_fail (GIT_IS_COMMIT_DIALOG (cdiag));
  g_return_if_fail (commit == NULL || GIT_IS_COMMIT (commit));

  priv = cdiag->priv;

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
