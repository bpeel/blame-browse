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

#ifndef __GIT_COMMIT_BAG_H__
#define __GIT_COMMIT_BAG_H__

#include <glib-object.h>
#include "git-commit.h"

G_BEGIN_DECLS

#define GIT_TYPE_COMMIT_BAG                                             \
  (git_commit_bag_get_type())
#define GIT_COMMIT_BAG(obj)                                             \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               GIT_TYPE_COMMIT_BAG,                     \
                               GitCommitBag))
#define GIT_COMMIT_BAG_CLASS(klass)                                     \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            GIT_TYPE_COMMIT_BAG,                        \
                            GitCommitBagClass))
#define GIT_IS_COMMIT_BAG(obj)                                          \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               GIT_TYPE_COMMIT_BAG))
#define GIT_IS_COMMIT_BAG_CLASS(klass)                                  \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            GIT_TYPE_COMMIT_BAG))
#define GIT_COMMIT_BAG_GET_CLASS(obj)                                   \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              GIT_TYPE_COMMIT_BAG,                      \
                              GitCommitBagClass))

typedef struct _GitCommitBag        GitCommitBag;
typedef struct _GitCommitBagClass   GitCommitBagClass;
typedef struct _GitCommitBagPrivate GitCommitBagPrivate;

struct _GitCommitBagClass
{
  GObjectClass parent_class;
};

struct _GitCommitBag
{
  GObject parent;

  GitCommitBagPrivate *priv;
};

GType git_commit_bag_get_type (void) G_GNUC_CONST;

GitCommitBag *git_commit_bag_get_default (void);

GitCommit *git_commit_bag_get (GitCommitBag *commit_bag,
                               const gchar *hash, const gchar *repo);

G_END_DECLS

#endif /* __GIT_COMMIT_BAG_H__ */

