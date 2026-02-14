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

#include "git-main-window.h"

int
main (int argc, char **argv)
{
  GtkWidget *main_win;
  const gchar *filename = NULL, *revision = NULL;

  g_set_application_name (_("Blame Browse"));

  gtk_init (&argc, &argv);

  if (argc > 1)
    {
      if (argc == 2)
        filename = argv[1];
      else if (argc == 3)
        {
          revision = argv[1];
          filename = argv[2];
        }
      else
        {
          fprintf (stderr, "usage: %s [revision] [filename]\n",
                   g_get_prgname ());
          return 1;
        }
    }

  main_win = git_main_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (main_win), 560, 460);
  g_signal_connect (main_win, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  if (filename)
    git_main_window_set_file (GIT_MAIN_WINDOW (main_win), filename, revision);

  gtk_widget_show (main_win);

  gtk_main ();

  return 0;
}
