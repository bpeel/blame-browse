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

#include "git-application.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "git-main-window.h"

struct _GitApplication
{
  GtkApplication parent_object;
};

G_DEFINE_TYPE (GitApplication,
               git_application,
               GTK_TYPE_APPLICATION);

static void
git_application_init (GitApplication *app)
{
}

static gboolean
git_application_local_command_line (GApplication *application,
                                    char ***arguments,
                                    int *exit_status)
{
  char **argv = *arguments;

  /* If there are two or more arguments then we’ll treat the first one
     as a revision and the second one as the filename. Otherwise we’ll
     let the default handler handle them. */
  if (argv[0] && argv[1] && argv[2])
    {
      if (argv[3])
        {
          g_printerr (_("usage: %s [revision] [filename]\n"), g_get_prgname ());
          *exit_status = 1;
          return TRUE;
        }

      GError *error = NULL;

      if (!g_application_register (application, NULL, &error))
        {
          g_printerr ("Failed to register: %s\n", error->message);
          g_error_free (error);
          *exit_status = 1;
          return TRUE;
        }

      GFile *file = g_file_new_for_commandline_arg (argv[2]);

      g_application_open (application, &file, 1, argv[1]);

      g_object_unref (file);

      *exit_status = 0;
      return TRUE;
    }

  return G_APPLICATION_CLASS (git_application_parent_class)
    ->local_command_line(application, arguments, exit_status);
}

static void
git_application_open (GApplication *application,
                      GFile **files,
                      int n_files,
                      const char *hint)
{
  const char *revision = hint && *hint ? hint : NULL;

  for (int i = 0; i < n_files; i++)
    {
      GtkWidget *main_win = git_main_window_new (GTK_APPLICATION (application));

      git_main_window_set_file (GIT_MAIN_WINDOW (main_win), files[i], revision);

      gtk_window_present (GTK_WINDOW (main_win));
    }
}

static void
git_application_activate (GApplication *application)
{
  GtkWidget *main_win = git_main_window_new (GTK_APPLICATION (application));

  gtk_window_present (GTK_WINDOW (main_win));
}

static void
git_application_class_init (GitApplicationClass *klass)
{
  GApplicationClass *app_class = (GApplicationClass *) klass;

  app_class->local_command_line = git_application_local_command_line;
  app_class->open = git_application_open;
  app_class->activate = git_application_activate;
}

GitApplication *
git_application_new (void)
{
  return g_object_new (GIT_TYPE_APPLICATION,
                       "application-id", "uk.co.busydoingnothing.blame_browser",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}
