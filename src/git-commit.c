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

#include <glib-object.h>
#include <string.h>

#include "git-commit.h"
#include "git-reader.h"
#include "git-common.h"
#include "git-commit-bag.h"

#define GIT_COMMIT_DEFAULT_HASH "0000000000000000000000000000000000000000"

static void git_commit_finalize (GObject *object);
static void git_commit_dispose (GObject *object);
static void git_commit_set_property (GObject *object, guint property_id,
                                     const GValue *value, GParamSpec *pspec);
static void git_commit_get_property (GObject *object, guint property_id,
                                     GValue *value, GParamSpec *pspec);
static void git_commit_unref_reader (GitCommit *commit);
static void git_commit_free_parents (GitCommit *commit);

G_DEFINE_TYPE (GitCommit, git_commit, G_TYPE_OBJECT);

#define GIT_COMMIT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GIT_TYPE_COMMIT, \
                                GitCommitPrivate))

struct _GitCommitPrivate
{
  gchar *hash, *repo;
  GHashTable *props;

  gboolean has_log_data;
  GSList *parents;
  gchar *log_data;

  GitReader *reader;
  guint line_handler, completed_handler;
  GString *log_buf;
  gboolean got_parents;
};

enum
  {
    PROP_0,

    PROP_HASH,
    PROP_REPO,
    PROP_HAS_LOG_DATA
  };

static void
git_commit_class_init (GitCommitClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GParamSpec *pspec;

  gobject_class->finalize = git_commit_finalize;
  gobject_class->dispose = git_commit_dispose;
  gobject_class->set_property = git_commit_set_property;
  gobject_class->get_property = git_commit_get_property;

  pspec = g_param_spec_string ("hash",
                               "Commit hash",
                               "The SHA1 hash used to name the commit",
                               GIT_COMMIT_DEFAULT_HASH,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READABLE
                               | G_PARAM_WRITABLE);
  g_object_class_install_property (gobject_class, PROP_HASH, pspec);

  pspec = g_param_spec_string ("repo",
                               "repository",
                               "Location of the repository where this commit "
                               "originated",
                               ".",
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READABLE
                               | G_PARAM_WRITABLE);
  g_object_class_install_property (gobject_class, PROP_REPO, pspec);

  pspec = g_param_spec_boolean ("has-log-data",
                                "has log data",
                                "Whether the log data for the commit has "
                                "been retrieved",
                                FALSE,
                                G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_HAS_LOG_DATA, pspec);

  g_type_class_add_private (klass, sizeof (GitCommitPrivate));
}

static void
git_commit_init (GitCommit *self)
{
  GitCommitPrivate *priv;

  priv = self->priv = GIT_COMMIT_GET_PRIVATE (self);

  priv->props = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free, g_free);
}

static void
git_commit_finalize (GObject *object)
{
  GitCommit *self = (GitCommit *) object;
  GitCommitPrivate *priv = self->priv;

  if (priv->hash)
    g_free (priv->hash);
  if (priv->repo)
    g_free (priv->repo);
  if (priv->log_buf)
    g_string_free (priv->log_buf, TRUE);
  g_hash_table_destroy (priv->props);

  G_OBJECT_CLASS (git_commit_parent_class)->finalize (object);
}

static void
git_commit_dispose (GObject *object)
{
  GitCommit *self = (GitCommit *) object;

  git_commit_unref_reader (self);
  git_commit_free_parents (self);

  G_OBJECT_CLASS (git_commit_parent_class)->finalize (object);
}

static void
git_commit_set_property (GObject *object, guint property_id,
                         const GValue *value, GParamSpec *pspec)
{
  GitCommit *commit = (GitCommit *) object;
  GitCommitPrivate *priv = commit->priv;

  switch (property_id)
    {
    case PROP_HASH:
      if (priv->hash)
        g_free (priv->hash);
      priv->hash = g_strdup (g_value_get_string (value));
      break;

    case PROP_REPO:
      if (priv->repo)
        g_free (priv->repo);
      priv->repo = g_strdup (g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
git_commit_get_property (GObject *object, guint property_id,
                         GValue *value, GParamSpec *pspec)
{
  GitCommit *commit = (GitCommit *) object;
  GitCommitPrivate *priv = commit->priv;

  switch (property_id)
    {
    case PROP_HASH:
      g_value_set_string (value, priv->hash);
      break;

    case PROP_REPO:
      g_value_set_string (value, priv->repo);

    case PROP_HAS_LOG_DATA:
      g_value_set_boolean (value, priv->has_log_data);

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

GitCommit *
git_commit_new (const gchar *hash, const gchar *repo)
{
  GitCommit *self = g_object_new (GIT_TYPE_COMMIT,
                                  "hash", hash,
                                  "repo", repo,
                                  NULL);

  return self;
}

const gchar *
git_commit_get_hash (GitCommit *commit)
{
  g_return_val_if_fail (GIT_IS_COMMIT (commit), NULL);

  return commit->priv->hash;
}

const gchar *
git_commit_get_repo (GitCommit *commit)
{
  g_return_val_if_fail (GIT_IS_COMMIT (commit), NULL);

  return commit->priv->repo;
}

gboolean
git_commit_get_has_log_data (GitCommit *commit)
{
  g_return_val_if_fail (GIT_IS_COMMIT (commit), FALSE);

  return commit->priv->has_log_data;
}

const gchar *
git_commit_get_log_data (GitCommit *commit)
{
  g_return_val_if_fail (GIT_IS_COMMIT (commit), NULL);
  g_return_val_if_fail (commit->priv->has_log_data, NULL);

  return commit->priv->log_data;
}

const GSList *
git_commit_get_parents (GitCommit *commit)
{
  g_return_val_if_fail (GIT_IS_COMMIT (commit), NULL);
  g_return_val_if_fail (commit->priv->has_log_data, NULL);

  return commit->priv->parents;
}

static void
git_commit_on_completed (GitReader *reader, const GError *error,
                         GitCommit *commit)
{
  GitCommitPrivate *priv = commit->priv;

  git_commit_unref_reader (commit);

  if (error)
    {
      priv->log_data = g_strdup (error->message);
      g_string_free (priv->log_buf, TRUE);
      priv->log_buf = NULL;
      git_commit_free_parents (commit);
    }
  else
    {
      priv->log_data = priv->log_buf->str;
      g_string_free (priv->log_buf, FALSE);
      priv->log_buf = NULL;
    }

  priv->has_log_data = TRUE;
  g_object_notify (G_OBJECT (commit), "has-log-data");
}

static gboolean
git_commit_on_line (GitReader *reader, guint length, const gchar *line,
                    GitCommit *commit)
{
  GitCommitPrivate *priv = commit->priv;
  gboolean ret = TRUE;

  if (priv->got_parents)
    g_string_append_len (priv->log_buf, line, length);
  else if (length < GIT_COMMIT_HASH_LENGTH + 7
           || memcmp (line, "commit ", 7)
           || strlen (priv->hash) != GIT_COMMIT_HASH_LENGTH
           || memcmp (line + 7, priv->hash, GIT_COMMIT_HASH_LENGTH))
    {
      GError *error = NULL;

      g_set_error (&error, GIT_ERROR, GIT_ERROR_PARSE_ERROR,
                   "Invalid output from git-log");
      git_commit_on_completed (priv->reader, error, commit);
      g_error_free (error);

      ret = FALSE;
    }
  else
    {
      GitCommitBag *commit_bag = git_commit_bag_get_default ();

      line += GIT_COMMIT_HASH_LENGTH + 7;
      length -= GIT_COMMIT_HASH_LENGTH + 7;

      while (length > GIT_COMMIT_HASH_LENGTH)
        {
          int i;
          gchar *hash;
          GitCommit *commit;

          if (*line != ' ')
            break;

          for (i = 1; i <= GIT_COMMIT_HASH_LENGTH
                 && ((line[i] >= 'a' && line[i] <= 'f')
                     || (line[i] >= '0' && line[i] <= '9'));
               i++);

          if (i <= GIT_COMMIT_HASH_LENGTH)
            break;

          hash = g_strndup (line + 1, GIT_COMMIT_HASH_LENGTH);
          commit = git_commit_bag_get (commit_bag, hash, priv->repo);
          g_free (hash);

          priv->parents = g_slist_prepend (priv->parents,
                                           g_object_ref (commit));

          length -= GIT_COMMIT_HASH_LENGTH + 1;
          line += GIT_COMMIT_HASH_LENGTH + 1;
        }

      if (length != 1 || *line != '\n')
        {
          GError *error = NULL;

          g_set_error (&error, GIT_ERROR, GIT_ERROR_PARSE_ERROR,
                       "Invalid output from git-log");
          git_commit_on_completed (priv->reader, error, commit);
          g_error_free (error);

          ret = FALSE;
        }
      else
        priv->got_parents = TRUE;
    }

  return ret;
}

void
git_commit_fetch_log_data (GitCommit *commit)
{
  GitCommitPrivate *priv;

  g_return_if_fail (GIT_IS_COMMIT (commit));

  priv = commit->priv;

  if (!priv->has_log_data && priv->reader == NULL)
    {
      GError *error = NULL;

      priv->reader = git_reader_new ();
      priv->log_buf = g_string_new ("");

      priv->line_handler
        = g_signal_connect (priv->reader, "line",
                            G_CALLBACK (git_commit_on_line), commit);
      priv->completed_handler
        = g_signal_connect (priv->reader, "completed",
                            G_CALLBACK (git_commit_on_completed), commit);

      git_reader_start (priv->reader, priv->repo, &error,
                        "log", "-n", "1", "--stat", "--parents",
                        priv->hash, NULL);

      if (error)
        {
          git_commit_on_completed (priv->reader, error, commit);
          g_error_free (error);
        }
    }
}

void
git_commit_set_prop (GitCommit *commit, const gchar *prop_name,
                     const gchar *value)
{
  g_return_if_fail (GIT_IS_COMMIT (commit));

  g_hash_table_insert (commit->priv->props,
                       g_strdup (prop_name),
                       g_strdup (value));
}

const gchar *
git_commit_get_prop (GitCommit *commit, const gchar *prop_name)
{
  g_return_val_if_fail (GIT_IS_COMMIT (commit), NULL);

  return g_hash_table_lookup (commit->priv->props, prop_name);
}

void
git_commit_get_color (GitCommit *commit, GdkColor *color)
{
  GitCommitPrivate *priv;
  int i;

  g_return_if_fail (GIT_IS_COMMIT (commit));

  priv = commit->priv;

  memset (color, 0, sizeof (GdkColor));

  /* Use the first 6 bytes of the commit hash as a colour */
  for (i = 0; i < 12; i++)
    {
      int nibble;

      if ((priv->hash[i] < 'a' || priv->hash[i] > 'f')
          && (priv->hash[i] < '0' || priv->hash[i] > '9'))
        {
          g_warning ("Invalid commit hash");
          return;
        }

      nibble = priv->hash[i] >= 'a'
        ? priv->hash[i] - 'a' + 10 : priv->hash[i] - '0';

      if (i < 4)
        color->red = (color->red << 4) | nibble;
      else if (i < 8)
        color->green = (color->green << 4) | nibble;
      else
        color->blue = (color->blue << 4) | nibble;
    }
}

static void
git_commit_unref_reader (GitCommit *commit)
{
  GitCommitPrivate *priv = commit->priv;

  if (priv->reader)
    {
      g_signal_handler_disconnect (priv->reader, priv->line_handler);
      g_signal_handler_disconnect (priv->reader, priv->completed_handler);
      g_object_unref (priv->reader);
      priv->reader = NULL;
    }
}

static void
git_commit_free_parents (GitCommit *commit)
{
  GitCommitPrivate *priv = commit->priv;

  g_slist_foreach (priv->parents, (GFunc) g_object_unref, NULL);
  g_slist_free (priv->parents);
  priv->parents = NULL;
}
