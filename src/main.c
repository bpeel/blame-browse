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
    {
      fprintf (stderr, "ERROR: %s\n", error->message);
      *(int *) user_data = 1;
    }

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

  g_signal_connect (source, "completed", G_CALLBACK (on_completed), &ret);

  if (!git_annotated_source_fetch (source, "main.c", "HEAD", &error))
    {
      fprintf (stderr, "ERROR: %s\n", error->message);
      g_error_free (error);
      ret = 1;
    }

  gtk_main ();
  
  if (ret == 0)
    {
      gsize i;

      for (i = 0; i < git_annotated_source_get_n_lines (source); i++)
	{
	  const GitAnnotatedSourceLine *line
	    = git_annotated_source_get_line (source, i);

	  printf ("%s:%s:%s:%i:%i:%s",
		  git_commit_get_hash (line->commit),
		  git_commit_get_prop (line->commit, "author"),
		  git_commit_get_prop (line->commit, "author-mail"),
		  line->orig_line,
		  line->final_line,
		  line->text);
	}
    }

  g_object_unref (source);

  return ret;
}
