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

#define GIT_TYPE_COMMIT_BAG git_commit_bag_get_type ()

G_DECLARE_FINAL_TYPE (GitCommitBag,
                      git_commit_bag,
                      GIT,
                      COMMIT_BAG,
                      GObject);

GitCommitBag *git_commit_bag_get_default (void);

GitCommit *git_commit_bag_get (GitCommitBag *commit_bag,
                               const gchar *hash, GFile *repo);

G_END_DECLS

#endif /* __GIT_COMMIT_BAG_H__ */

