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

#include <gtk/gtkmain.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkscrolledwindow.h>

#include "git-source-view.h"

int
main (int argc, char **argv)
{
  GtkWidget *win, *scrolled_win, *source_view;

  gtk_init (&argc, &argv);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 560, 460);
  g_signal_connect (win, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);

  source_view = git_source_view_new ();
  git_source_view_set_file (GIT_SOURCE_VIEW (source_view),
			    "git-annotated-source.c", NULL);
  gtk_widget_show (source_view);
  gtk_container_add (GTK_CONTAINER (scrolled_win), source_view);
  
  gtk_widget_show (scrolled_win);
  gtk_container_add (GTK_CONTAINER (win), scrolled_win);

  gtk_widget_show (win);

  gtk_main ();
  
  return 0;
}
