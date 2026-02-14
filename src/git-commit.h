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

#ifndef __GIT_COMMIT_H__
#define __GIT_COMMIT_H__

#include <glib-object.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GIT_TYPE_COMMIT git_commit_get_type()

G_DECLARE_FINAL_TYPE (GitCommit,
                      git_commit,
                      GIT,
                      COMMIT,
                      GObject);

#define GIT_COMMIT_HASH_LENGTH 40

GitCommit *git_commit_new (const gchar *hash, const gchar *repo);

const gchar *git_commit_get_hash (GitCommit *commit);
const gchar *git_commit_get_repo (GitCommit *commit);

gboolean git_commit_get_has_log_data (GitCommit *commit);
const gchar *git_commit_get_log_data (GitCommit *commit);
const GSList *git_commit_get_parents (GitCommit *commit);
void git_commit_fetch_log_data (GitCommit *commit);

void git_commit_set_prop (GitCommit *commit, const gchar *prop_name,
                          const gchar *value);
const gchar *git_commit_get_prop (GitCommit *commit, const gchar *prop_name);

void git_commit_get_color (GitCommit *commit, GdkRGBA *color);

G_END_DECLS

#endif /* __GIT_COMMIT_H__ */
