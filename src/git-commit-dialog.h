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

#ifndef __GIT_COMMIT_DIALOG_H__
#define __GIT_COMMIT_DIALOG_H__

#include <gtk/gtk.h>
#include "git-commit.h"

G_BEGIN_DECLS

#define GIT_TYPE_COMMIT_DIALOG git_commit_dialog_get_type ()

G_DECLARE_FINAL_TYPE (GitCommitDialog,
                      git_commit_dialog,
                      GIT,
                      COMMIT_DIALOG,
                      GtkDialog);

enum {
  GIT_COMMIT_DIALOG_RESPONSE_VIEW_BLAME
};

GtkWidget *git_commit_dialog_new (void);

void git_commit_dialog_set_commit (GitCommitDialog *cdiag,
                                   GitCommit *commit);
GitCommit *git_commit_dialog_get_commit (GitCommitDialog *cdiag);

G_END_DECLS

#endif /* __GIT_COMMIT_DIALOG_H__ */

