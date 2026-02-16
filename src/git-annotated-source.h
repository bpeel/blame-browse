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

#ifndef __GIT_ANNOTATED_SOURCE_H__
#define __GIT_ANNOTATED_SOURCE_H__

#include <glib-object.h>
#include "git-commit.h"

G_BEGIN_DECLS

#define GIT_TYPE_ANNOTATED_SOURCE git_annotated_source_get_type ()

G_DECLARE_DERIVABLE_TYPE (GitAnnotatedSource,
                          git_annotated_source,
                          GIT,
                          ANNOTATED_SOURCE,
                          GObject);

struct _GitAnnotatedSourceClass
{
  GObjectClass parent_class;

  void (* completed) (GitAnnotatedSource *source, const GError *error);
};

typedef struct _GitAnnotatedSourceLine
{
  GitCommit *commit;
  guint orig_line, final_line;
  gchar *text;
} GitAnnotatedSourceLine;

GitAnnotatedSource *git_annotated_source_new (void);
gboolean git_annotated_source_fetch (GitAnnotatedSource *source,
                                     GFile *file,
                                     const gchar *revision,
                                     GError **error);

gsize git_annotated_source_get_n_lines (GitAnnotatedSource *source);

const GitAnnotatedSourceLine *
git_annotated_source_get_line (GitAnnotatedSource *source, gsize line_num);

G_END_DECLS

#endif /* __GIT_ANNOTATED_SOURCE_H__ */
