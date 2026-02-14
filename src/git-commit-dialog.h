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

#define GIT_TYPE_COMMIT_DIALOG                                          \
  (git_commit_dialog_get_type())
#define GIT_COMMIT_DIALOG(obj)                                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               GIT_TYPE_COMMIT_DIALOG,                  \
                               GitCommitDialog))
#define GIT_COMMIT_DIALOG_CLASS(klass)                                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            GIT_TYPE_COMMIT_DIALOG,                     \
                            GitCommitDialogClass))
#define GIT_IS_COMMIT_DIALOG(obj)                                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               GIT_TYPE_COMMIT_DIALOG))
#define GIT_IS_COMMIT_DIALOG_CLASS(klass)                               \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            GIT_TYPE_COMMIT_DIALOG))
#define GIT_COMMIT_DIALOG_GET_CLASS(obj)                                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              GIT_TYPE_COMMIT_DIALOG,                   \
                              GitCommitDialogClass))

typedef struct _GitCommitDialog        GitCommitDialog;
typedef struct _GitCommitDialogClass   GitCommitDialogClass;
typedef struct _GitCommitDialogPrivate GitCommitDialogPrivate;

struct _GitCommitDialogClass
{
  GtkDialogClass parent_class;
};

struct _GitCommitDialog
{
  GtkDialog parent;

  GitCommitDialogPrivate *priv;
};

enum {
  GIT_COMMIT_DIALOG_RESPONSE_VIEW_BLAME
};

GType git_commit_dialog_get_type (void) G_GNUC_CONST;

GtkWidget *git_commit_dialog_new (void);

void git_commit_dialog_set_commit (GitCommitDialog *cdiag,
                                   GitCommit *commit);
GitCommit *git_commit_dialog_get_commit (GitCommitDialog *cdiag);

G_END_DECLS

#endif /* __GIT_COMMIT_DIALOG_H__ */

