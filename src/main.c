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
  g_signal_connect (win, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);

  source_view = git_source_view_new ();
  git_source_view_set_file (GIT_SOURCE_VIEW (source_view),
			    "git-annotated-source.c", "HEAD");
  gtk_widget_show (source_view);
  gtk_container_add (GTK_CONTAINER (scrolled_win), source_view);
  
  gtk_widget_show (scrolled_win);
  gtk_container_add (GTK_CONTAINER (win), scrolled_win);

  gtk_widget_show (win);

  gtk_main ();
  
  return 0;
}
