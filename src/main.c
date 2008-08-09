#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "git-annotated-source.h"

int
main (int argc, char **argv)
{
  GitAnnotatedSource *source;
  int ret = 0;
  GError *error = NULL;
  GMainLoop *main_loop;

  g_type_init ();

  source = git_annotated_source_new ();

  if (!git_annotated_source_fetch (source, "main.c", "HEAD", &error))
    {
      fprintf (stderr, "ERROR: %s\n", error->message);
      g_error_free (error);
      ret = 1;
    }

  main_loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (main_loop);

  g_object_unref (source);

  return ret;
}
