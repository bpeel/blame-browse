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

#include "git-commit-bag.h"

#include <glib-object.h>

#include "git-commit.h"

static void git_commit_bag_dispose (GObject *object);
static void git_commit_bag_finalize (GObject *object);

struct _GitCommitBag
{
  GObject parent;
};

typedef struct
{
  GHashTable *hash_table;
} GitCommitBagPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (GitCommitBag,
                                  git_commit_bag,
                                  G_TYPE_OBJECT);

static void
git_commit_bag_class_init (GitCommitBagClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = git_commit_bag_dispose;
  gobject_class->finalize = git_commit_bag_finalize;
}

static void
git_commit_bag_init (GitCommitBag *self)
{
  GitCommitBagPrivate *priv = git_commit_bag_get_instance_private (self);

  priv->hash_table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            g_free, g_object_unref);
}

static void
git_commit_bag_dispose (GObject *object)
{
  GitCommitBag *self = (GitCommitBag *) object;
  GitCommitBagPrivate *priv = git_commit_bag_get_instance_private (self);

  g_hash_table_remove_all (priv->hash_table);

  G_OBJECT_CLASS (git_commit_bag_parent_class)->dispose (object);
}

static void
git_commit_bag_finalize (GObject *object)
{
  GitCommitBag *self = (GitCommitBag *) object;
  GitCommitBagPrivate *priv = git_commit_bag_get_instance_private (self);

  g_hash_table_destroy (priv->hash_table);

  G_OBJECT_CLASS (git_commit_bag_parent_class)->finalize (object);
}

GitCommitBag *
git_commit_bag_get_default (void)
{
  static GitCommitBag *default_bag = NULL;

  if (default_bag == NULL)
    default_bag = g_object_new (GIT_TYPE_COMMIT_BAG, NULL);

  return default_bag;
}

GitCommit *
git_commit_bag_get (GitCommitBag *commit_bag, const gchar *hash,
                    GFile *repo)
{
  g_return_val_if_fail (GIT_IS_COMMIT_BAG (commit_bag), NULL);

  GitCommitBagPrivate *priv = git_commit_bag_get_instance_private (commit_bag);
  gchar *hash_copy = g_strdup (hash);

  GitCommit *commit;

  if ((commit = g_hash_table_lookup (priv->hash_table, hash_copy)))
    g_free (hash_copy);
  else
    {
      commit = git_commit_new (hash_copy, repo);
      g_hash_table_insert (priv->hash_table, hash_copy, commit);
    }

  return commit;
}
