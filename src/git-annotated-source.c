#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <glib.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>

#include "git-annotated-source.h"

static void git_annotated_source_dispose (GObject *object);
static void git_annotated_source_finalize (GObject *object);

G_DEFINE_TYPE (GitAnnotatedSource, git_annotated_source, G_TYPE_OBJECT);

#define GIT_ANNOTATED_SOURCE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GIT_TYPE_ANNOTATED_SOURCE, \
				GitAnnotatedSourcePrivate))

struct _GitAnnotatedSourcePrivate
{
  gboolean has_child;
  GPid child_pid;
  GIOChannel *child_stdout;
  GIOChannel *child_stderr;
  guint child_watch_source;
  guint child_stdout_source;
  guint child_stderr_source;
};

static void
git_annotated_source_class_init (GitAnnotatedSourceClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = git_annotated_source_dispose;
  gobject_class->finalize = git_annotated_source_finalize;

  g_type_class_add_private (klass, sizeof (GitAnnotatedSourcePrivate));
}

static void
git_annotated_source_init (GitAnnotatedSource *self)
{
  self->priv = GIT_ANNOTATED_SOURCE_GET_PRIVATE (self);
}

static void
git_annotated_source_close_process (GitAnnotatedSource *source,
				    gboolean kill_child)
{
  GitAnnotatedSourcePrivate *priv = source->priv;

  if (priv->has_child)
    {
      priv->has_child = FALSE;

      if (priv->child_stdout_source)
	g_source_remove (priv->child_stdout_source);
      if (priv->child_stderr_source)
	g_source_remove (priv->child_stderr_source);

      g_io_channel_shutdown (priv->child_stdout, FALSE, NULL);
      g_io_channel_unref (priv->child_stdout);
      g_io_channel_shutdown (priv->child_stderr, FALSE, NULL);
      g_io_channel_unref (priv->child_stderr);

      if (priv->child_pid)
	{
	  if (kill_child)
	    {
	      int status_ret, wait_ret;

	      g_source_remove (priv->child_watch_source);

	      /* Check if the process has already terminated */
	      while ((wait_ret = waitpid (priv->child_pid, &status_ret,
					  WNOHANG)) == -1
		     && errno == EINTR);

	      /* Otherwise try killing it */
	      if (wait_ret == 0)
		{
		  kill (priv->child_pid, SIGTERM);

		  while ((wait_ret = waitpid (priv->child_pid, &status_ret,
					      0)) == -1
			 && errno == EINTR);
		}	  
	    }

	  priv->child_pid = 0;
	  g_spawn_close_pid (priv->child_pid);
	}
    }  
}

static void
git_annotated_source_dispose (GObject *object)
{
  GitAnnotatedSource *self = (GitAnnotatedSource *) object;

  git_annotated_source_close_process (self, TRUE);

  G_OBJECT_CLASS (git_annotated_source_parent_class)->dispose (object);
}

static void
git_annotated_source_finalize (GObject *object)
{
  GitAnnotatedSource *self = (GitAnnotatedSource *) object;

  G_OBJECT_CLASS (git_annotated_source_parent_class)->finalize (object);
}

GitAnnotatedSource *
git_annotated_source_new (void)
{
  GitAnnotatedSource *self = g_object_new (GIT_TYPE_ANNOTATED_SOURCE, NULL);

  return self;
}

static void
git_annotated_source_check_complete (GitAnnotatedSource *source)
{
  GitAnnotatedSourcePrivate *priv = source->priv;

  if (priv->child_pid == 0
      && priv->child_stdout_source == 0
      && priv->child_stderr_source == 0)
    {
      printf ("completed!\n");
      git_annotated_source_close_process (source, FALSE);
    }
}

static void
git_annotated_source_on_child_exit (GPid pid, gint status, gpointer data)
{
  GitAnnotatedSource *source = (GitAnnotatedSource *) data;
  GitAnnotatedSourcePrivate *priv = source->priv;

  g_spawn_close_pid (pid);
  priv->child_pid = 0;

  git_annotated_source_check_complete (source);
}

static void
git_annotated_source_on_read_error (GitAnnotatedSource *source, GError *error)
{
  fprintf (stderr, "read error: %s\n", error->message);

  g_error_free (error);

  git_annotated_source_close_process (source, TRUE);
}

static gboolean
git_annotated_source_on_child_stdout (GIOChannel *io_source,
				      GIOCondition condition, gpointer data)
{
  GitAnnotatedSource *source = (GitAnnotatedSource *) data;
  GitAnnotatedSourcePrivate *priv = source->priv;
  GError *error = NULL;
  gchar buf[512];
  gsize bytes_read;
  gboolean ret = TRUE;

  switch (g_io_channel_read_chars (io_source, buf, sizeof (buf),
				   &bytes_read, &error))
    {
    case G_IO_STATUS_ERROR:
      git_annotated_source_on_read_error (source, error);
      ret = FALSE;
      break;

    case G_IO_STATUS_NORMAL:
      fputs ("got stdout: \"", stdout);
      fwrite (buf, bytes_read, 1, stdout);
      fputs ("\"\n", stdout);
      break;

    case G_IO_STATUS_EOF:
      priv->child_stdout_source = 0;
      git_annotated_source_check_complete (source);
      ret = FALSE;
      break;
      
    case G_IO_STATUS_AGAIN:
      break;
    }

  if (!ret)
    priv->child_stdout_source = 0;

  return ret;
}

static gboolean
git_annotated_source_on_child_stderr (GIOChannel *io_source,
				      GIOCondition condition, gpointer data)
{
  GitAnnotatedSource *source = (GitAnnotatedSource *) data;
  GitAnnotatedSourcePrivate *priv = source->priv;
  GError *error = NULL;
  gchar buf[512];
  gsize bytes_read;
  gboolean ret = TRUE;

  switch (g_io_channel_read_chars (io_source, buf, sizeof (buf),
				   &bytes_read, &error))
    {
    case G_IO_STATUS_ERROR:
      git_annotated_source_on_read_error (source, error);
      ret = FALSE;
      break;

    case G_IO_STATUS_NORMAL:
      fputs ("got stderr: \"", stdout);
      fwrite (buf, bytes_read, 1, stdout);
      fputs ("\"\n", stdout);
      break;

    case G_IO_STATUS_EOF:
      priv->child_stderr_source = 0;
      git_annotated_source_check_complete (source);
      ret = FALSE;
      break;
      
    case G_IO_STATUS_AGAIN:
      break;
    }

  if (!ret)
    priv->child_stderr_source = 0;

  return ret;
}

gboolean
git_annotated_source_fetch (GitAnnotatedSource *source,
			    const gchar *filename,
			    const gchar *revision,
			    GError **error)
{
  GitAnnotatedSourcePrivate *priv;
  gchar *args[5];
  gboolean spawn_ret;
  gint stdout_fd, stderr_fd;
  
  g_return_val_if_fail (GIT_IS_ANNOTATED_SOURCE (source), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  priv = source->priv;

  args[0] = "git-blame";
  args[1] = "-p";
  args[2] = g_strdup (filename);
  args[3] = g_strdup (revision);
  args[4] = NULL;

  git_annotated_source_close_process (source, TRUE);

  spawn_ret = g_spawn_async_with_pipes (NULL, args, NULL,
					G_SPAWN_SEARCH_PATH
					| G_SPAWN_DO_NOT_REAP_CHILD,
					NULL, NULL, &priv->child_pid,
					NULL, &stdout_fd, &stderr_fd,
					error);

  g_free (args[2]);
  g_free (args[3]);

  if (!spawn_ret)
    return FALSE;

  priv->child_watch_source
    = g_child_watch_add (priv->child_pid,
			 git_annotated_source_on_child_exit,
			 source);

  priv->child_stdout = g_io_channel_unix_new (stdout_fd);
  /* We want unbuffered data otherwise the call to read will block */
  g_io_channel_set_encoding (priv->child_stdout, NULL, NULL);
  g_io_channel_set_buffered (priv->child_stdout, FALSE);
  priv->child_stdout_source
    = g_io_add_watch (priv->child_stdout, G_IO_IN | G_IO_HUP | G_IO_ERR,
		      git_annotated_source_on_child_stdout,
		      source);

  priv->child_stderr = g_io_channel_unix_new (stderr_fd);
  /* We want unbuffered data otherwise the call to read will block */
  g_io_channel_set_encoding (priv->child_stderr, NULL, NULL);
  g_io_channel_set_buffered (priv->child_stderr, FALSE);
  priv->child_stderr_source
    = g_io_add_watch (priv->child_stderr, G_IO_IN | G_IO_HUP | G_IO_ERR,
		      git_annotated_source_on_child_stderr,
		      source);

  priv->has_child = TRUE;

  return TRUE;
}
