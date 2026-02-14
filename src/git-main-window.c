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
#include <string.h>

#include "git-main-window.h"
#include "git-source-view.h"
#include "git-commit-dialog.h"
#include "git-common.h"

typedef struct _GitMainWindowHistoryItem GitMainWindowHistoryItem;

static void git_main_window_dispose (GObject *object);
static void git_main_window_finalize (GObject *object);

static void git_main_window_update_source_state (GitMainWindow *main_window);
static void git_main_window_update_history_actions (GitMainWindow *main_window);

static void git_main_window_on_commit_selected (GitSourceView *sview,
                                                GitCommit *commit,
                                                GitMainWindow *main_window);

static GtkUIManager *git_main_window_create_ui_manager (void);

static void git_main_window_free_history_item (GitMainWindowHistoryItem *item);

static void git_main_window_on_open (GtkAction *action,
                                     GitMainWindow *main_window);
static void git_main_window_on_quit (GtkAction *action,
                                     GitMainWindow *main_window);
static void git_main_window_on_about (GtkAction *action,
                                      GitMainWindow *main_window);
static void git_main_window_on_back (GtkAction *action,
                                     GitMainWindow *main_window);
static void git_main_window_on_forward (GtkAction *action,
                                        GitMainWindow *main_window);

static void git_main_window_on_revision (GtkEntry *entry,
                                         GitMainWindow *main_window);

struct _GitMainWindow
{
  GtkWindow parent;
};

typedef struct
{
  GtkWidget *revision_bar, *source_view, *statusbar;
  GtkWidget *commit_dialog, *file_dialog;

  guint source_state_context;
  guint source_state_id;
  guint source_state_handler;
  guint commit_selected_handler;
  guint commit_response_handler;
  guint file_response_handler;
  guint revision_activated_handler;

  GList *history;
  GList *history_pos;

  GtkAction *back_action, *forward_action;
} GitMainWindowPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (GitMainWindow,
                                  git_main_window,
                                  GTK_TYPE_WINDOW);

struct _GitMainWindowHistoryItem
{
  gchar *filename;
  gchar *revision;
};

static GtkActionEntry
git_main_window_actions[] =
  {
    { "File", NULL, N_("_File"), NULL,
      NULL, NULL },
    { "Go", NULL, N_("_Go"), NULL,
      NULL, NULL },
    { "Help", NULL, N_("_Help"), NULL,
      NULL, NULL },
    { "FileOpen", GTK_STOCK_OPEN, N_("_Open"), NULL,
      NULL, G_CALLBACK (git_main_window_on_open) },
    { "FileQuit", GTK_STOCK_QUIT, N_("_Quit"), NULL,
      NULL, G_CALLBACK (git_main_window_on_quit) },
    { "HelpAbout", GTK_STOCK_ABOUT, N_("_About"), NULL,
      NULL, G_CALLBACK (git_main_window_on_about) },
    { "GoBack", GTK_STOCK_GO_BACK, N_("_Back"), "<Alt>Left",
      N_("Go back to previously visited commit"),
      G_CALLBACK (git_main_window_on_back) },
    { "GoForward", GTK_STOCK_GO_FORWARD, N_("_Forward"), "<Alt>Right",
      N_("Go forward to a previously visited commit"),
      G_CALLBACK (git_main_window_on_forward) },
  };

static void
git_main_window_class_init (GitMainWindowClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = git_main_window_dispose;
  gobject_class->finalize = git_main_window_finalize;
}

static void
git_main_window_init (GitMainWindow *self)
{
  GitMainWindowPrivate *priv = git_main_window_get_instance_private (self);
  GtkWidget *layout, *scrolled_win;
  GtkUIManager *ui_manager;

  layout = gtk_vbox_new (FALSE, 0);

  if ((ui_manager = git_main_window_create_ui_manager ()))
    {
      GtkWidget *widget;
      GtkActionGroup *action_group;
      GtkAccelGroup *accel_group;

      accel_group = gtk_accel_group_new ();

      action_group = gtk_action_group_new ("GitActions");
      gtk_action_group_add_actions (action_group, git_main_window_actions,
                                    G_N_ELEMENTS (git_main_window_actions),
                                    self);

      if ((priv->back_action = gtk_action_group_get_action (action_group,
                                                            "GoBack")))
        g_object_ref (priv->back_action);
      if ((priv->forward_action = gtk_action_group_get_action (action_group,
                                                               "GoForward")))
        g_object_ref (priv->forward_action);

      gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

      if ((widget = gtk_ui_manager_get_widget (ui_manager, "/menubar")))
        gtk_box_pack_start (GTK_BOX (layout), widget,
                            FALSE, FALSE, 0);

      if ((widget = gtk_ui_manager_get_widget (ui_manager, "/toolbar")))
        {
          GtkToolItem *tool_item = gtk_tool_item_new ();

          gtk_box_pack_start (GTK_BOX (layout), widget,
                              FALSE, FALSE, 0);

          priv->revision_bar = g_object_ref_sink (gtk_entry_new ());
          priv->revision_activated_handler
            = g_signal_connect (priv->revision_bar, "activate",
                                G_CALLBACK (git_main_window_on_revision),
                                self);
          gtk_widget_show (priv->revision_bar);
          gtk_container_add (GTK_CONTAINER (tool_item), priv->revision_bar);
          gtk_tool_item_set_expand (tool_item, TRUE);
          gtk_tool_item_set_tooltip_text (tool_item,
                                          _("Revision"));
          gtk_widget_show (GTK_WIDGET (tool_item));
          gtk_toolbar_insert (GTK_TOOLBAR (widget), tool_item, -1);
        }

      accel_group = gtk_ui_manager_get_accel_group (ui_manager);
      gtk_window_add_accel_group (GTK_WINDOW (self), accel_group);

      g_object_unref (action_group);

      g_object_unref (ui_manager);
    }

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  priv->source_view = g_object_ref_sink (git_source_view_new ());
  priv->source_state_handler = g_signal_connect_swapped
    (priv->source_view, "notify::state",
     G_CALLBACK (git_main_window_update_source_state), self);
  priv->commit_selected_handler = g_signal_connect
    (priv->source_view, "commit-selected",
     G_CALLBACK (git_main_window_on_commit_selected), self);
  gtk_widget_show (priv->source_view);
  gtk_container_add (GTK_CONTAINER (scrolled_win), priv->source_view);

  gtk_widget_show (scrolled_win);
  gtk_box_pack_start (GTK_BOX (layout), scrolled_win, TRUE, TRUE, 0);

  priv->statusbar = g_object_ref_sink (gtk_statusbar_new ());
  priv->source_state_context
    = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar),
                                    "source-state");
  gtk_widget_show (priv->statusbar);
  gtk_box_pack_start (GTK_BOX (layout), priv->statusbar, FALSE, FALSE, 0);

  gtk_widget_show (layout);
  gtk_container_add (GTK_CONTAINER (self), layout);

  git_main_window_update_source_state (self);
  git_main_window_update_history_actions (self);
}

static void
git_main_window_dispose (GObject *object)
{
  GitMainWindow *self = (GitMainWindow *) object;
  GitMainWindowPrivate *priv = git_main_window_get_instance_private (self);

  if (priv->source_view)
    {
      g_signal_handler_disconnect (priv->source_view,
                                   priv->source_state_handler);
      g_signal_handler_disconnect (priv->source_view,
                                   priv->commit_selected_handler);
      g_object_unref (priv->source_view);
      priv->source_view = NULL;
    }

  if (priv->revision_bar)
    {
      g_signal_handler_disconnect (priv->revision_bar,
                                   priv->revision_activated_handler);
      g_object_unref (priv->revision_bar);
      priv->revision_bar = NULL;
    }

  if (priv->statusbar)
    {
      g_object_unref (priv->statusbar);
      priv->statusbar = NULL;
    }

  if (priv->commit_dialog)
    {
      g_signal_handler_disconnect (priv->commit_dialog,
                                   priv->commit_response_handler);
      g_object_unref (priv->commit_dialog);
      priv->commit_dialog = NULL;
    }

  if (priv->file_dialog)
    {
      g_object_unref (priv->file_dialog);
      priv->file_dialog = NULL;
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
git_main_window_new (void)
{
  GtkWidget *self = g_object_new (GIT_TYPE_MAIN_WINDOW,
                                  "type", GTK_WINDOW_TOPLEVEL,
                                  NULL);

  return self;
}

static void
git_main_window_do_set_file (GitMainWindow *main_window,
                             const gchar *filename,
                             const gchar *revision)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->source_view)
    git_source_view_set_file (GIT_SOURCE_VIEW (priv->source_view),
                              filename, revision);

  if (priv->revision_bar)
    gtk_entry_set_text (GTK_ENTRY (priv->revision_bar),
                        revision ? revision : "");
}

static void
git_main_window_free_history_item (GitMainWindowHistoryItem *item)
{
  if (item->filename)
    g_free (item->filename);
  if (item->revision)
    g_free (item->revision);
  g_slice_free (GitMainWindowHistoryItem, item);
}

static void
git_main_window_add_history (GitMainWindow *main_window,
                             const gchar *filename,
                             const gchar *revision)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);
  GitMainWindowHistoryItem *item;

  item = g_slice_new (GitMainWindowHistoryItem);
  item->filename = g_strdup (filename);
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
                          const gchar *filename,
                          const gchar *revision)
{
  g_return_if_fail (GIT_IS_MAIN_WINDOW (main_window));
  g_return_if_fail (filename != NULL);

  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  git_main_window_do_set_file (main_window, filename, revision);

  git_main_window_add_history (main_window, filename, revision);

  if (priv->source_view)
    gtk_widget_grab_focus (priv->source_view);
}

static void
git_main_window_update_source_state (GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->statusbar && priv->source_view)
    {
      if (priv->source_state_id)
        {
          gtk_statusbar_remove (GTK_STATUSBAR (priv->statusbar),
                                priv->source_state_context,
                                priv->source_state_id);
          priv->source_state_id = 0;
        }

      switch (git_source_view_get_state (GIT_SOURCE_VIEW (priv->source_view)))
        {
        case GIT_SOURCE_VIEW_READY:
          break;

        case GIT_SOURCE_VIEW_ERROR:
          {
            const GError *error
              = git_source_view_get_state_error (GIT_SOURCE_VIEW
                                                 (priv->source_view));

            if (error)
              priv->source_state_id
                = gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar),
                                      priv->source_state_context,
                                      error->message);
            else
              priv->source_state_id
                = gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar),
                                      priv->source_state_context,
                                      _("Error"));
          }
          break;

        case GIT_SOURCE_VIEW_LOADING:
          priv->source_state_id
            = gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar),
                                  priv->source_state_context,
                                  _("Loading..."));
          break;
        }
    }
}

static void
git_main_window_update_history_actions (GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->back_action)
    gtk_action_set_sensitive (priv->back_action,
                              priv->history_pos && priv->history_pos->prev);
  if (priv->forward_action)
    gtk_action_set_sensitive (priv->forward_action,
                              priv->history_pos && priv->history_pos->next);

  if (priv->revision_bar)
    gtk_widget_set_sensitive (priv->revision_bar,
                              priv->history_pos != NULL);
}

static void
git_main_window_on_commit_response (GtkDialog *dialog, gint response,
                                    GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (response == GIT_COMMIT_DIALOG_RESPONSE_VIEW_BLAME
      && priv->history_pos)
    {
      GitCommit *commit
        = git_commit_dialog_get_commit (GIT_COMMIT_DIALOG (dialog));
      GitMainWindowHistoryItem *item
        = (GitMainWindowHistoryItem *) priv->history_pos->data;

      if (commit)
        git_main_window_set_file (main_window, item->filename,
                                  git_commit_get_hash (commit));
    }

  gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
git_main_window_on_commit_selected (GitSourceView *sview,
                                    GitCommit *commit,
                                    GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->commit_dialog == NULL)
    {
      priv->commit_dialog = g_object_ref_sink (git_commit_dialog_new ());
      gtk_window_set_default_size (GTK_WINDOW (priv->commit_dialog),
                                   -1, 400);

      /* Keep the window alive when it is closed */
      g_signal_connect (priv->commit_dialog, "delete_event",
                        G_CALLBACK (gtk_widget_hide_on_delete), NULL);

      priv->commit_response_handler
        = g_signal_connect (priv->commit_dialog, "response",
                            G_CALLBACK (git_main_window_on_commit_response),
                            main_window);
    }

  g_object_set (priv->commit_dialog, "commit", commit, NULL);

  gtk_window_present (GTK_WINDOW (priv->commit_dialog));
}

static GtkUIManager *
git_main_window_create_ui_manager (void)
{
  GtkUIManager *ui_manager;
  GError *error = NULL;

  ui_manager = gtk_ui_manager_new ();

  /* Try the source dir first */
  if (!gtk_ui_manager_add_ui_from_file (ui_manager,
                                        BB_SRCDIR "/data/main-win-ui.xml",
                                        &error))
    {
      /* If the file doesn't exist then try the package data folder */
      if (error->domain == G_FILE_ERROR && error->code == G_FILE_ERROR_NOENT)
        {
          g_clear_error (&error);
          gtk_ui_manager_add_ui_from_file (ui_manager,
                                           BB_DATADIR
                                           "/ui/main-win-ui.xml",
                                           &error);
        }
    }

  if (error)
    {
      g_warning ("Failed to load main-win-ui.xml: %s", error->message);
      g_error_free (error);
      g_object_unref (ui_manager);
      ui_manager = NULL;
    }

  return ui_manager;
}

static void
git_main_window_on_file_response (GtkDialog *dialog, gint response,
                                  GitMainWindow *main_window)
{
  if (response == GTK_RESPONSE_OK)
    {
      gchar *filename
        = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

      if (filename)
        git_main_window_set_file (main_window, filename, NULL);

      g_free (filename);
    }

  gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
git_main_window_on_open (GtkAction *action,
                         GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->file_dialog == NULL)
    {
      priv->file_dialog
        = gtk_file_chooser_dialog_new (_("Open File"),
                                       GTK_WINDOW (main_window),
                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                       GTK_STOCK_OK, GTK_RESPONSE_OK,
                                       NULL);
      g_object_ref_sink (priv->file_dialog);

      /* Keep the window alive when it is closed */
      g_signal_connect (priv->file_dialog, "delete_event",
                        G_CALLBACK (gtk_widget_hide_on_delete), NULL);

      priv->file_response_handler
        = g_signal_connect (priv->file_dialog, "response",
                            G_CALLBACK (git_main_window_on_file_response),
                            main_window);
    }

  if (priv->history_pos)
    {
      GitMainWindowHistoryItem *item
        = (GitMainWindowHistoryItem *) priv->history_pos->data;
      gchar *dir = g_path_get_dirname (item->filename);

      if (!g_path_is_absolute (dir))
        {
          gchar *current_dir = g_get_current_dir ();
          gchar *full_dir = g_build_filename (current_dir, dir, NULL);

          g_free (dir);
          dir = full_dir;
          g_free (current_dir);
        }

      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (priv->file_dialog),
                                           dir);

      g_free (dir);
    }

  gtk_window_present (GTK_WINDOW (priv->file_dialog));
}

static void
git_main_window_on_quit (GtkAction *action,
                         GitMainWindow *main_window)
{
  gtk_widget_destroy (GTK_WIDGET (main_window));
}

static void
git_main_window_on_about (GtkAction *action,
                          GitMainWindow *main_window)
{
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
git_main_window_on_back (GtkAction *action,
                         GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->history_pos && priv->history_pos->prev)
    {
      GitMainWindowHistoryItem *item;
      priv->history_pos = priv->history_pos->prev;
      item = (GitMainWindowHistoryItem *) priv->history_pos->data;
      git_main_window_do_set_file (main_window, item->filename, item->revision);

      git_main_window_update_history_actions (main_window);
    }
}

static void
git_main_window_on_forward (GtkAction *action,
                            GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->history_pos && priv->history_pos->next)
    {
      GitMainWindowHistoryItem *item;
      priv->history_pos = priv->history_pos->next;
      item = (GitMainWindowHistoryItem *) priv->history_pos->data;
      git_main_window_do_set_file (main_window, item->filename, item->revision);

      git_main_window_update_history_actions (main_window);
    }
}

static void
git_main_window_on_revision (GtkEntry *entry,
                             GitMainWindow *main_window)
{
  GitMainWindowPrivate *priv =
    git_main_window_get_instance_private (main_window);

  if (priv->history_pos)
    {
      GitMainWindowHistoryItem *item
        = (GitMainWindowHistoryItem *) priv->history_pos->data;
      const gchar *revision = gtk_entry_get_text (entry);

      git_main_window_set_file (main_window, item->filename,
                                strlen (revision) ? revision : NULL);
    }
}
