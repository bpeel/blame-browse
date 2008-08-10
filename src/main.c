#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <gtk/gtkmain.h>

#include "git-annotated-source.h"

static void
on_completed (GitAnnotatedSource *source,
	      const GError *error,
	      gpointer user_data)
{
  if (error)
    fprintf (stderr, "ERROR: %s\n", error->message);

  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  GitAnnotatedSource *source;
  int ret = 0;
  GError *error = NULL;

  gtk_init (&argc, &argv);

  source = git_annotated_source_new ();

  g_signal_connect (source, "completed", G_CALLBACK (on_completed), NULL);

  if (!git_annotated_source_fetch (source, "main.c", "HEAD", &error))
    {
      fprintf (stderr, "ERROR: %s\n", error->message);
      g_error_free (error);
      ret = 1;
    }

  gtk_main ();

  g_object_unref (source);

  return ret;
}
