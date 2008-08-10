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
#include <string.h>

#include "git-annotated-source.h"
#include "git-reader.h"
#include "git-commit.h"
#include "git-common.h"

static void git_annotated_source_dispose (GObject *object);
static void git_annotated_source_finalize (GObject *object);

static void
git_annotated_source_on_reader_completed (GitReader *reader,
					  const GError *error,
					  GitAnnotatedSource *source);
static gboolean
git_annotated_source_on_line (GitReader *reader,
			      guint length, const gchar *str,
			      GitAnnotatedSource *source);

G_DEFINE_TYPE (GitAnnotatedSource, git_annotated_source, G_TYPE_OBJECT);

#define GIT_ANNOTATED_SOURCE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GIT_TYPE_ANNOTATED_SOURCE, \
				GitAnnotatedSourcePrivate))

struct _GitAnnotatedSourcePrivate
{
  GitReader *reader;
  guint completed_handler;
  guint line_handler;

  GArray *lines;
  GitAnnotatedSourceLine current_line;

  GHashTable *commit_bag;
};

enum
  {
    COMPLETED,

    LAST_SIGNAL
  };

static guint client_signals[LAST_SIGNAL];

static void
git_annotated_source_class_init (GitAnnotatedSourceClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = git_annotated_source_dispose;
  gobject_class->finalize = git_annotated_source_finalize;

  client_signals[COMPLETED]
    = g_signal_new ("completed",
		    G_TYPE_FROM_CLASS (gobject_class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GitAnnotatedSourceClass, completed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1,
		    G_TYPE_POINTER);

  g_type_class_add_private (klass, sizeof (GitAnnotatedSourcePrivate));
}

static void
git_annotated_source_init (GitAnnotatedSource *self)
{
  GitAnnotatedSourcePrivate *priv;

  priv = self->priv = GIT_ANNOTATED_SOURCE_GET_PRIVATE (self);

  priv->reader = git_reader_new ();

  priv->completed_handler
    = g_signal_connect (priv->reader, "completed",
			G_CALLBACK (git_annotated_source_on_reader_completed),
			self);

  priv->line_handler
    = g_signal_connect (priv->reader, "line",
			G_CALLBACK (git_annotated_source_on_line),
			self);

  priv->lines = g_array_new (FALSE, FALSE, sizeof (GitAnnotatedSourceLine));
  priv->current_line.commit = NULL;
  priv->current_line.text = NULL;

  priv->commit_bag = g_hash_table_new_full (g_str_hash, g_str_equal,
					    g_free, g_object_unref);
}

static void
git_annotated_source_clear_lines (GitAnnotatedSource *source)
{
  GitAnnotatedSourcePrivate *priv = source->priv;
  int i;

  for (i = 0; i < priv->lines->len; i++)
    {
      GitAnnotatedSourceLine *line
	= &g_array_index (priv->lines, GitAnnotatedSourceLine, i);

      g_object_unref (line->commit);
      g_free (line->text);
    }

  g_array_set_size (priv->lines, 0);

  if (priv->current_line.commit)
    {
      g_object_unref (priv->current_line.commit);
      priv->current_line.commit = NULL;
    }
  if (priv->current_line.text)
    {
      g_free (priv->current_line.text);
      priv->current_line.text = NULL;
    }

  g_hash_table_remove_all (priv->commit_bag);
}

static void
git_annotated_source_dispose (GObject *object)
{
  GitAnnotatedSource *self = (GitAnnotatedSource *) object;
  GitAnnotatedSourcePrivate *priv = self->priv;

  if (priv->reader)
    {
      g_signal_handler_disconnect (priv->reader, priv->completed_handler);
      g_signal_handler_disconnect (priv->reader, priv->line_handler);
      g_object_unref (priv->reader);
      priv->reader = NULL;
    }

  G_OBJECT_CLASS (git_annotated_source_parent_class)->dispose (object);
}

static void
git_annotated_source_finalize (GObject *object)
{
  GitAnnotatedSource *self = (GitAnnotatedSource *) object;
  GitAnnotatedSourcePrivate *priv = self->priv;

  git_annotated_source_clear_lines (self);
  g_array_free (priv->lines, TRUE);
  g_hash_table_destroy (priv->commit_bag);

  G_OBJECT_CLASS (git_annotated_source_parent_class)->finalize (object);
}

GitAnnotatedSource *
git_annotated_source_new (void)
{
  GitAnnotatedSource *self = g_object_new (GIT_TYPE_ANNOTATED_SOURCE, NULL);

  return self;
}

const GitAnnotatedSourceLine *
git_annotated_source_get_line (GitAnnotatedSource *source,
			       gsize line_num)
{
  GitAnnotatedSourcePrivate *priv;

  g_return_val_if_fail (GIT_IS_ANNOTATED_SOURCE (source), NULL);
  priv = source->priv;
  g_return_val_if_fail (line_num >= 0 && line_num < priv->lines->len, NULL);

  return &g_array_index (priv->lines, GitAnnotatedSourceLine, line_num);
}

gsize
git_annotated_source_get_n_lines (GitAnnotatedSource *source)
{
  GitAnnotatedSourcePrivate *priv;

  g_return_val_if_fail (GIT_IS_ANNOTATED_SOURCE (source), 0);

  priv = source->priv;

  return priv->lines->len;
}

gboolean
git_annotated_source_fetch (GitAnnotatedSource *source,
			    const gchar *filename,
			    const gchar *revision,
			    GError **error)
{
  GitAnnotatedSourcePrivate *priv;
  gchar *dir_part, *base_part;
  gboolean ret;

  g_return_val_if_fail (GIT_IS_ANNOTATED_SOURCE (source), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (source->priv->reader != NULL, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  priv = source->priv;

  git_annotated_source_clear_lines (source);

  dir_part = g_path_get_dirname (filename);
  base_part = g_path_get_basename (filename);

  /* Revision can be NULL in which case it will terminate the argument
     list early and git will include uncommitted changes */
  ret = git_reader_start (priv->reader, dir_part, error, "blame", "-p",
			  base_part, revision, NULL);

  g_free (dir_part);
  g_free (base_part);

  return ret;
}

static void
git_annotated_source_parse_error (GitAnnotatedSource *source)
{
  GError *error = NULL;

  g_set_error (&error, GIT_ERROR, GIT_ERROR_PARSE_ERROR,
	       "Invalid data from git-blame received");

  g_signal_emit (source, client_signals[COMPLETED], 0, error);

  g_error_free (error);
}

static void
git_annotated_source_on_reader_completed (GitReader *reader,
					  const GError *error,
					  GitAnnotatedSource *source)
{
  /* If we've got a commit for the current line then we must be
     missing the actual code for the line so the output is invalid */
  if (source->priv->current_line.commit)
    git_annotated_source_parse_error (source);
  else
    g_signal_emit (source, client_signals[COMPLETED], 0, error);
}

static gboolean
git_annotated_source_on_line (GitReader *reader,
			      guint length, const gchar *str,
			      GitAnnotatedSource *source)
{
  GitAnnotatedSourcePrivate *priv = source->priv;
  int i;
  const gchar *p = str;
  gboolean ret = TRUE;

  /* If we haven't got a commit yet then we are expecting the first
     line to be the commit hash followed by two or three numbers for
     the lines */
  if (priv->current_line.commit == NULL)
    {
      int nums[3];

      if (length < GIT_COMMIT_HASH_LENGTH)
	{
	  git_annotated_source_parse_error (source);
	  ret = FALSE;
	}
      else
	{
	  for (i = 0; i < GIT_COMMIT_HASH_LENGTH; i++, p++)
	    if ((*p < '0' || *p > '9') && (*p < 'a' || *p > 'f'))
	      break;

	  length -= GIT_COMMIT_HASH_LENGTH;

	  if (i < GIT_COMMIT_HASH_LENGTH)
	    {
	      git_annotated_source_parse_error (source);
	      ret = FALSE;
	    }
	  else
	    {
	      for (i = 0; i < 3; i++)
		{
		  nums[i] = 0;
		  if (length < 1 || *p != ' ')
		    {
		      git_annotated_source_parse_error (source);
		      ret = FALSE;
		      break;
		    }
		  length--;
		  p++;
		  while (length > 0 && *p >= '0' && *p <= '9')
		    {
		      nums[i] = nums[i] * 10 + *p - '0';
		      length--;
		      p++;
		    }
		  /* The last number is optional */
		  if (i == 1 && length == 1 && *p == '\n')
		    break;
		}

	      if (ret)
		{
		  if (length != 1 || *p != '\n')
		    {
		      git_annotated_source_parse_error (source);
		      ret = FALSE;
		    }
		  else
		    {
		      gchar *hash = g_strndup (str, GIT_COMMIT_HASH_LENGTH);
		      GitCommit *commit;

		      if ((commit = g_hash_table_lookup (priv->commit_bag,
							 hash)))
			g_free (hash);
		      else
			{
			  commit = git_commit_new (hash);
			  g_hash_table_insert (priv->commit_bag, hash, commit);
			}
		      priv->current_line.commit = g_object_ref (commit);
		      priv->current_line.orig_line = nums[0];
		      priv->current_line.final_line = nums[1];
		    }
		}
	    }
	}
    }
  /* If this is the code of the line then it begins with a tab */
  else if (length >= 1 && *str == '\t')
    {
      priv->current_line.text = g_strndup (str + 1, length - 1);
      g_array_append_val (priv->lines, priv->current_line);
      priv->current_line.commit = NULL;
      priv->current_line.text = NULL;
    }
  /* Otherwise it should be a key-value property pair */
  else
    {
      const gchar *sep;
      
      if (length > 1 && str[length - 1] == '\n')
	length--;

      if ((sep = memchr (str, ' ', length)))
	{
	  gchar *key = g_strndup (str, sep - str);
	  gchar *value = g_strndup (sep + 1, str + length - sep - 1);

	  git_commit_set_prop (priv->current_line.commit, key, value);

	  g_free (key);
	  g_free (value);
	}
    }

  return ret;
}
