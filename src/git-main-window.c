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

#include "git-main-window.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include "git-source-view.h"
#include "git-commit-dialog.h"
#include "git-common.h"

typedef struct _GitMainWindowHistoryItem GitMainWindowHistoryItem;

static void git_main_window_dispose (GObject *object);
static void git_main_window_finalize (GObject *object);

static void git_main_window_update_history_actions (GitMainWindow *main_window);

static void git_main_window_on_commit_selected (GitSourceView *source_view,
                                                GitCommit *commit,
                                                GitMainWindow *main_window);
static void git_main_window_on_view_blame (GitCommitDialog *cdiag,
                                           GitCommit *commit,
                                           GitMainWindow *main_window);

static void git_main_window_free_history_item (GitMainWindowHistoryItem *item);

static void git_main_window_on_open (GSimpleAction *action,
                                     GVariant *parameter,
                                     gpointer user_data);
static void git_main_window_on_about (GSimpleAction *action,
                                      GVariant *parameter,
                                      gpointer user_data);
static void git_main_window_on_back (GSimpleAction *action,
                                     GVariant *parameter,
                                     gpointer user_data);
static void git_main_window_on_forward (GSimpleAction *action,
                                        GVariant *parameter,
                                        gpointer user_data);

static void git_main_window_on_revision (GtkText *entry,
                                         GitMainWindow *main_window);

struct _GitMainWindow
{
  GtkApplicationWindow parent;
};

typedef struct
{
  GtkWidget *revision_bar, *source_view, *menu_button;
  GtkWidget *commit_dialog;

  GCancellable *file_dialog_cancellable;

  guint commit_selected_handler;
  guint view_blame_handler;
  guint revision_activated_handler;

  GList *history;
  GList *history_pos;

  GAction *back_action, *forward_action;
} GitMainWindowPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (GitMainWindow,
                                  git_main_window,
                                  GTK_TYPE_APPLICATION_WINDOW);

struct _GitMainWindowHistoryItem
{
  GFile *file;
  gchar *revision;
};

static GActionEntry
git_main_window_actions[] =
  {
    { .name = "open", .activate = git_main_window_on_open },
    { .name = "about", .activate = git_main_window_on_about },
    { .name = "back", .activate = git_main_window_on_back },
    { .name = "forward", .activate = git_main_window_on_forward },
  };

static void
git_main_window_class_init (GitMainWindowClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = git_main_window_dispose;
  gobject_class->finalize = git_main_window_finalize;

  const char *resource_name = "/uk/co/busydoingnothing/blamebrowse/window.ui";

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                               resource_name);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitMainWindow,
                                                source_view);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitMainWindow,
                                                revision_bar);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                GitMainWindow,
                                                menu_button);
}

static void
git_main_window_init (GitMainWindow *self)
{
  GitMainWindowPrivate *priv = git_main_window_get_instance_private (self);

  g_type_ensure (GIT_TYPE_SOURCE_VIEW);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   git_main_window_actions,
                                   G_N_ELEMENTS (git_main_window_actions),
                                   self);

  priv->back_action =
    g_object_ref (g_action_map_lookup_action (G_ACTION_MAP (self), "back"));
  priv->forward_action =
    g_object_ref (g_action_map_lookup_action (G_ACTION_MAP (self), "forward"));

  priv->revision_activated_handler
    = g_signal_connect (priv->revision_bar, "activate",
                        G_CALLBACK (git_main_window_on_revision),
                        self);

  priv->commit_selected_handler = g_signal_connect
    (priv->source_view, "commit-selected",
     G_CALLBACK (git_main_window_on_commit_selected), self);

  const char *menu_resource = "/uk/co/busydoingnothing/blamebrowse/menu.ui";
  GtkBuilder *builder = gtk_builder_new_from_resource (menu_resource);
  GMenuModel *menu_model
    = G_MENU_MODEL (gtk_builder_get_object (builder, "menu"));
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->menu_button),
                                  menu_model);
  g_object_unref (builder);

  git_main_window_update_history_actions (self);
}

static void
git_main_window_dispose (GObject *object)
{
  GitMainWindow *self = (GitMainWindow *) object;
  GitMainWindowPrivate *priv = git_main_window_get_instance_private (self);

  if (priv->file_dialog_cancellable)
    {
      g_cancellable_cancel (priv->file_dialog_cancellable);
      g_clear_object (&priv->file_dialog_cancellable);
    }

  if (priv->source_view)
    {
      g_signal_handler_disconnect (priv->source_view,
                                   priv->commit_selected_handler);
    }

  if (priv->revision_bar)
    {
      g_signal_handler_disconnect (priv->revision_bar,
                                   priv->revision_activated_handler);
    }

  if (priv->commit_dialog)
    {
      g_signal_handler_disconnect (priv->commit_dialog,
                                   priv->view_blame_handler);
      g_object_unref (priv->commit_dialog);
      priv->commit_dialog = NULL;
    }

  if (priv->back_action)
    {
      g_object_unref (priv->back_action);
      priv->back_action = NULL;
    }
  if (priv->forward_action)
    {
      g_object_unref (priv->forward_action);
      priv->forward_action = NULL;
    }

  gtk_widget_dispose_template (GTK_WIDGET (self), GIT_TYPE_MAIN_WINDOW);

  G_OBJECT_CLASS (git_main_window_parent_class)->dispose (object);
}

static void
git_main_window_finalize (GObject *object)
{
  GitMainWindow *self = (GitMainWindow *) object;
  GitMainWindowPrivate *priv = git_main_window_get_instance_private (self);

  g_list_foreach (priv->history, (GFunc) git_main_window_free_history_item,
                  NULL);
  g_list_free (priv->history);

  G_OBJECT_CLASS (git_main_window_parent_class)->finalize (object);
}

GtkWidget *
git_main_window_new (GtkApplication *app)
{
  GtkWidget *self = g_object_new (GIT_TYPE_MAIN_WINDOW,
                                  "application", app,
                                  NULL);

  return self;
}

static void
git_main_window_do_set_file (GitMainWindow *main_window,
                             GFile *file,
                             const gchar *revision)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->source_view)
    git_source_view_set_file (GIT_SOURCE_VIEW (priv->source_view),
                              file, revision);

  if (priv->revision_bar)
    gtk_editable_set_text (GTK_EDITABLE (priv->revision_bar),
                           revision ? revision : "");
}

static void
git_main_window_free_history_item (GitMainWindowHistoryItem *item)
{
  if (item->file)
    g_object_unref (item->file);
  if (item->revision)
    g_free (item->revision);
  g_slice_free (GitMainWindowHistoryItem, item);
}

static void
git_main_window_add_history (GitMainWindow *main_window,
                             GFile *file,
                             const gchar *revision)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);
  GitMainWindowHistoryItem *item;

  item = g_slice_new (GitMainWindowHistoryItem);
  item->file = g_object_ref (file);
  item->revision = revision ? g_strdup (revision) : NULL;

  if (priv->history_pos == NULL)
    priv->history = priv->history_pos = g_list_prepend (NULL, item);
  else
    {
      /* Destroy the forward list */
      g_list_foreach (priv->history_pos->next,
                      (GFunc) git_main_window_free_history_item,
                      NULL);
      g_list_free (priv->history_pos->next);
      priv->history_pos->next = NULL;

      priv->history_pos = g_list_append (priv->history_pos, item);

      priv->history_pos = priv->history_pos->next;
    }

  git_main_window_update_history_actions (main_window);
}

void
git_main_window_set_file (GitMainWindow *main_window,
                          GFile *file,
                          const gchar *revision)
{
  g_return_if_fail (GIT_IS_MAIN_WINDOW (main_window));
  g_return_if_fail (file != NULL);

  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  git_main_window_do_set_file (main_window, file, revision);

  git_main_window_add_history (main_window, file, revision);

  if (priv->source_view)
    gtk_widget_grab_focus (priv->source_view);
}

static void
git_main_window_update_history_actions (GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->back_action)
    g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->back_action),
                                 priv->history_pos && priv->history_pos->prev);
  if (priv->forward_action)
    g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->forward_action),
                                 priv->history_pos && priv->history_pos->next);

  if (priv->revision_bar)
    gtk_widget_set_sensitive (priv->revision_bar,
                              priv->history_pos != NULL);
}

static void
git_main_window_on_view_blame (GitCommitDialog *cdiag,
                               GitCommit *commit,
                               GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->history_pos)
    {
      GitMainWindowHistoryItem *item
        = (GitMainWindowHistoryItem *) priv->history_pos->data;

      git_main_window_set_file (main_window, item->file,
                                git_commit_get_hash (commit));
    }

  gtk_widget_set_visible (GTK_WIDGET (cdiag), FALSE);
}

static void
git_main_window_on_commit_selected (GitSourceView *source_view,
                                    GitCommit *commit,
                                    GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->commit_dialog == NULL)
    {
      priv->commit_dialog = git_commit_dialog_new (GTK_WINDOW (main_window));
      g_object_ref_sink (priv->commit_dialog);
      gtk_window_set_default_size (GTK_WINDOW (priv->commit_dialog),
                                   -1, 400);

      /* Keep the window alive when it is closed */
      gtk_window_set_hide_on_close (GTK_WINDOW (priv->commit_dialog), TRUE);

      priv->view_blame_handler = g_signal_connect
              (priv->commit_dialog, "view-blame",
               G_CALLBACK (git_main_window_on_view_blame),
               main_window);
    }

  g_object_set (priv->commit_dialog, "commit", commit, NULL);

  gtk_window_present (GTK_WINDOW (priv->commit_dialog));
}

static void
git_main_window_file_dialog_ready (GObject *source,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
  GitMainWindow *main_window = user_data;
  GitMainWindowPrivate *priv
    = git_main_window_get_instance_private (main_window);

  g_clear_object (&priv->file_dialog_cancellable);

  GFile *file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source),
                                             result,
                                             NULL /* error */);

  if (file)
    {
      git_main_window_set_file (main_window, file, NULL);

      g_object_unref (file);
    }
}

static void
git_main_window_on_open (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
  GitMainWindow *main_window = user_data;
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->file_dialog_cancellable)
    {
      /* There’s already a file dialog open */
      return;
    }

  priv->file_dialog_cancellable = g_cancellable_new ();

  GtkFileDialog *dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_modal (dialog, FALSE);
  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (main_window),
                        priv->file_dialog_cancellable,
                        git_main_window_file_dialog_ready,
                        main_window);
  g_object_unref (dialog);
}

static void
git_main_window_on_about (GSimpleAction *action,
                          GVariant *parameter,
                          gpointer user_data)
{
  GitMainWindow *main_window = user_data;

  const gchar *license_text =
    _("This program is free software: you can redistribute it and/or "
      "modify it under the terms of the GNU General Public License as "
      "published by the Free Software Foundation, either version 3 of "
      "the License, or (at your option) any later version.");
  const gchar *authors[] =
    {
      "Neil Roberts <bpeeluk@yahoo.co.uk>",
      NULL
    };
  const gchar *copyright = "Copyright \xc2\xa9 2008 Neil Roberts";

  gtk_show_about_dialog (GTK_WINDOW (main_window),
                         "version", PACKAGE_VERSION,
                         "website",
                         "http://www.busydoingnothing.co.uk/blame-browse/",
                         "comments", _("GUI front-end for git-blame"),
                         "license", license_text,
                         "wrap-license", TRUE,
                         "authors", authors,
                         "copyright", copyright,
                         NULL);
}

static void
git_main_window_on_back (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
  GitMainWindow *main_window = user_data;
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->history_pos && priv->history_pos->prev)
    {
      GitMainWindowHistoryItem *item;
      priv->history_pos = priv->history_pos->prev;
      item = (GitMainWindowHistoryItem *) priv->history_pos->data;
      git_main_window_do_set_file (main_window, item->file, item->revision);

      git_main_window_update_history_actions (main_window);
    }
}

static void
git_main_window_on_forward (GSimpleAction *action,
                            GVariant *parameter,
                            gpointer user_data)
{
  GitMainWindow *main_window = user_data;
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->history_pos && priv->history_pos->next)
    {
      GitMainWindowHistoryItem *item;
      priv->history_pos = priv->history_pos->next;
      item = (GitMainWindowHistoryItem *) priv->history_pos->data;
      git_main_window_do_set_file (main_window, item->file, item->revision);

      git_main_window_update_history_actions (main_window);
    }
}

static void
git_main_window_on_revision (GtkText *entry,
                             GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->history_pos)
    {
      GitMainWindowHistoryItem *item
        = (GitMainWindowHistoryItem *) priv->history_pos->data;
      const gchar *entry_text = gtk_editable_get_text (GTK_EDITABLE (entry));

      /* Copy the entry text because we will end up using to update
       * the entry text and that will go wrong if we use the pointer
       * into the entry’s buffer. */
      char *revision = strlen (entry_text) ? g_strdup (entry_text) : NULL;

      git_main_window_set_file (main_window, item->file,
                                strlen (revision) ? revision : NULL);

      g_free (revision);
    }
}
