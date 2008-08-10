#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtkmain.h>
#include <gtk/gtkwindow.h>

#include "git-source-view.h"

int
main (int argc, char **argv)
{
  GtkWidget *win, *source_view;

  gtk_init (&argc, &argv);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (win, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  source_view = git_source_view_new ();
  git_source_view_set_file (GIT_SOURCE_VIEW (source_view), "main.c", "HEAD");
  gtk_widget_show (source_view);
  gtk_container_add (GTK_CONTAINER (win), source_view);

  gtk_widget_show (win);

  gtk_main ();
  
  return 0;
}
