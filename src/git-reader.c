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

#include <glib-object.h>
#include <glib.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "git-reader.h"
#include "git-common.h"
#include "git-marshal.h"

static void git_reader_dispose (GObject *object);
static void git_reader_finalize (GObject *object);
static gboolean git_reader_default_line (GitReader *reader,
                                         guint length, const gchar *string);

G_DEFINE_TYPE (GitReader, git_reader, G_TYPE_OBJECT);

#define GIT_READER_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GIT_TYPE_READER, \
                                GitReaderPrivate))

struct _GitReaderPrivate
{
  gboolean has_child;
  GPid child_pid;
  GIOChannel *child_stdout;
  GIOChannel *child_stderr;
  guint child_watch_source;
  guint child_stdout_source;
  guint child_stderr_source;
  gint child_exit_code;
  GString *error_string;
  GString *line_string;
};

enum
  {
    COMPLETED,
    LINE,

    LAST_SIGNAL
  };

static guint client_signals[LAST_SIGNAL];

static void
git_reader_class_init (GitReaderClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = git_reader_dispose;
  gobject_class->finalize = git_reader_finalize;
  klass->line = git_reader_default_line;

  client_signals[COMPLETED]
    = g_signal_new ("completed",
                    G_TYPE_FROM_CLASS (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GitReaderClass, completed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__POINTER,
                    G_TYPE_NONE, 1,
                    G_TYPE_POINTER);

  client_signals[LINE]
    = g_signal_new ("line",
                    G_TYPE_FROM_CLASS (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GitReaderClass, line),
                    _git_boolean_continue_accumulator, NULL,
                    _git_marshal_BOOLEAN__UINT_STRING,
                    G_TYPE_BOOLEAN, 2,
                    G_TYPE_UINT,
                    G_TYPE_STRING);

  g_type_class_add_private (klass, sizeof (GitReaderPrivate));
}

static void
git_reader_init (GitReader *self)
{
  GitReaderPrivate *priv;

  priv = self->priv = GIT_READER_GET_PRIVATE (self);

  priv->error_string = g_string_new ("");
  priv->line_string = g_string_new ("");
}

static void
git_reader_close_process (GitReader *source,
                          gboolean kill_child)
{
  GitReaderPrivate *priv = source->priv;

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
git_reader_dispose (GObject *object)
{
  GitReader *self = (GitReader *) object;

  git_reader_close_process (self, TRUE);

  G_OBJECT_CLASS (git_reader_parent_class)->dispose (object);
}

static void
git_reader_finalize (GObject *object)
{
  GitReader *self = (GitReader *) object;

  g_string_free (self->priv->error_string, TRUE);
  g_string_free (self->priv->line_string, TRUE);

  G_OBJECT_CLASS (git_reader_parent_class)->finalize (object);
}

static gboolean
git_reader_default_line (GitReader *reader,
                         guint length, const gchar *string)
{
  /* Don't do anything but continue emission */
  return TRUE;
}

GitReader *
git_reader_new (void)
{
  GitReader *self = g_object_new (GIT_TYPE_READER, NULL);

  return self;
}

static void
git_reader_check_complete (GitReader *reader)
{
  GitReaderPrivate *priv = reader->priv;

  g_object_ref (reader);

  if (priv->child_pid == 0
      && priv->child_stdout_source == 0
      && priv->child_stderr_source == 0)
    {
      gboolean line_return = TRUE;

      /* If there's any data left in the line buffer then it
         represents a line with no terminator but it will probably
         still want to be handled so we should emit the signal */
      if (priv->line_string->len > 0)
        g_signal_emit (reader, client_signals[LINE], 0,
                       priv->line_string->len, priv->line_string->str,
                       &line_return);

      git_reader_close_process (reader, FALSE);

      /* Don't fire the completed signal if line signal halted
         processing */
      if (line_return)
        {
          GError *error = NULL;

          if (priv->child_exit_code)
            {
              gssize len;

              /* Remove spaces at the end of the error string */
              for (len = priv->error_string->len;
                   len > 0 && isspace (priv->error_string->str[len - 1]);
                   len--);
              g_string_truncate (priv->error_string, len);

              if (len > 0)
                g_set_error (&error, GIT_ERROR, GIT_ERROR_EXIT_STATUS,
                             "Error invoking git: %s",
                             priv->error_string->str);
              else
                g_set_error (&error, GIT_ERROR, GIT_ERROR_EXIT_STATUS,
                             "Error invoking git");
            }

          g_signal_emit (reader, client_signals[COMPLETED], 0, error);

          if (error)
            g_error_free (error);
        }
    }

  g_object_unref (reader);
}

static gboolean
git_reader_check_lines (GitReader *reader)
{
  GitReaderPrivate *priv = reader->priv;
  gboolean line_return = TRUE;
  gchar *start = priv->line_string->str, *end;
  gsize len = priv->line_string->len;
  gboolean ret = TRUE;

  g_object_ref (reader);

  while ((end = memchr (start, '\n', len)))
    {
      g_signal_emit (reader, client_signals[LINE], 0,
                     end - start + 1, start, &line_return);

      len -= end - start + 1;
      start = end + 1;

      if (!line_return)
        {
          git_reader_close_process (reader, TRUE);
          ret = FALSE;
          break;
        }
    }

  /* Move the remaining incomplete line to the beginning of the
     string */
  memmove (priv->line_string->str, start, len);
  g_string_truncate (priv->line_string, len);

  g_object_unref (reader);

  return ret;
}

static void
git_reader_on_child_exit (GPid pid, gint status, gpointer data)
{
  GitReader *reader = (GitReader *) data;
  GitReaderPrivate *priv = reader->priv;

  g_spawn_close_pid (pid);
  priv->child_pid = 0;

  priv->child_exit_code = status;

  git_reader_check_complete (reader);
}

static void
git_reader_on_read_error (GitReader *reader, GError *error)
{
  git_reader_close_process (reader, TRUE);

  g_signal_emit (reader, client_signals[COMPLETED], 0, error);

  g_error_free (error);
}

static gboolean
git_reader_on_child_stdout (GIOChannel *io_source,
                            GIOCondition condition, gpointer data)
{
  GitReader *reader = (GitReader *) data;
  GitReaderPrivate *priv = reader->priv;
  GError *error = NULL;
  gchar buf[512];
  gsize bytes_read;
  gboolean ret = TRUE;

  switch (g_io_channel_read_chars (io_source, buf, sizeof (buf),
                                   &bytes_read, &error))
    {
    case G_IO_STATUS_ERROR:
      git_reader_on_read_error (reader, error);
      ret = FALSE;
      break;

    case G_IO_STATUS_NORMAL:
      g_string_append_len (priv->line_string, buf, bytes_read);
      ret = git_reader_check_lines (reader);
      break;

    case G_IO_STATUS_EOF:
      priv->child_stdout_source = 0;
      git_reader_check_complete (reader);
      ret = FALSE;
      break;

    case G_IO_STATUS_AGAIN:
      break;
    }

  return ret;
}

static gboolean
git_reader_on_child_stderr (GIOChannel *io_source,
                            GIOCondition condition, gpointer data)
{
  GitReader *reader = (GitReader *) data;
  GitReaderPrivate *priv = reader->priv;
  GError *error = NULL;
  gchar buf[512];
  gsize bytes_read;
  gboolean ret = TRUE;

  switch (g_io_channel_read_chars (io_source, buf, sizeof (buf),
                                   &bytes_read, &error))
    {
    case G_IO_STATUS_ERROR:
      git_reader_on_read_error (reader, error);
      ret = FALSE;
      break;

    case G_IO_STATUS_NORMAL:
      g_string_append_len (priv->error_string, buf, bytes_read);
      break;

    case G_IO_STATUS_EOF:
      priv->child_stderr_source = 0;
      git_reader_check_complete (reader);
      ret = FALSE;
      break;

    case G_IO_STATUS_AGAIN:
      break;
    }

  return ret;
}

gboolean
git_reader_start (GitReader *reader,
                  const gchar *working_directory,
                  GError **error,
                  ...)
{
  GitReaderPrivate *priv;
  gchar **args;
  const gchar *arg;
  gboolean spawn_ret;
  gint stdout_fd, stderr_fd;
  va_list ap_copy, ap;
  int argc = 1, i;

  g_return_val_if_fail (GIT_IS_READER (reader), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  priv = reader->priv;

  git_reader_close_process (reader, TRUE);

  /* Count the number of arguments */
  va_start (ap, error);
  G_VA_COPY (ap_copy, ap);
  while ((arg = va_arg (ap, const gchar *)))
    argc++;

  /* Copy the arguments to a string array */
  args = g_new (gchar *, argc + 1);
  args[0] = g_strdup ("git");
  i = 1;
  while ((arg = va_arg (ap_copy, const gchar *)))
    args[i++] = g_strdup (arg);
  args[i] = NULL;

  va_end (ap);

  spawn_ret = g_spawn_async_with_pipes (working_directory, args, NULL,
                                        G_SPAWN_SEARCH_PATH
                                        | G_SPAWN_DO_NOT_REAP_CHILD,
                                        NULL, NULL, &priv->child_pid,
                                        NULL, &stdout_fd, &stderr_fd,
                                        error);

  g_strfreev (args);

  if (!spawn_ret)
    return FALSE;

  priv->child_watch_source
    = g_child_watch_add (priv->child_pid,
                         git_reader_on_child_exit,
                         reader);

  priv->child_stdout = g_io_channel_unix_new (stdout_fd);
  /* We want unbuffered data otherwise the call to read will block */
  g_io_channel_set_encoding (priv->child_stdout, NULL, NULL);
  g_io_channel_set_buffered (priv->child_stdout, FALSE);
  priv->child_stdout_source
    = g_io_add_watch (priv->child_stdout, G_IO_IN | G_IO_HUP | G_IO_ERR,
                      git_reader_on_child_stdout,
                      reader);

  priv->child_stderr = g_io_channel_unix_new (stderr_fd);
  /* We want unbuffered data otherwise the call to read will block */
  g_io_channel_set_encoding (priv->child_stderr, NULL, NULL);
  g_io_channel_set_buffered (priv->child_stderr, FALSE);
  priv->child_stderr_source
    = g_io_add_watch (priv->child_stderr, G_IO_IN | G_IO_HUP | G_IO_ERR,
                      git_reader_on_child_stderr,
                      reader);

  priv->has_child = TRUE;

  g_string_truncate (priv->error_string, 0);
  g_string_truncate (priv->line_string, 0);

  return TRUE;
}
