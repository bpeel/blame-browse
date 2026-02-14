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

#ifndef __GIT_HASH_VIEW_H__
#define __GIT_HASH_VIEW_H__

#include <gtk/gtk.h>
#include "git-commit.h"
#include "git-annotated-source.h"

G_BEGIN_DECLS

#define GIT_TYPE_HASH_VIEW git_hash_view_get_type ()

G_DECLARE_DERIVABLE_TYPE (GitHashView,
                          git_hash_view,
                          GIT,
                          HASH_VIEW,
                          GtkWidget);

struct _GitHashViewClass
{
  GtkWidgetClass parent_class;

  void (* commit_selected) (GitHashView *hash_view,
                            GitCommit *commit);
};

GtkWidget *git_hash_view_new (GtkTextView *hview);

void git_hash_view_set_text_view (GitHashView *hview,
                                  GtkTextView *tview);

void git_hash_view_set_source (GitHashView *hview,
                               GitAnnotatedSource *source);

G_END_DECLS

#endif /* __GIT_HASH_VIEW_H__ */
