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

#ifndef __GIT_COMMIT_LINK_BUTTON_H__
#define __GIT_COMMIT_LINK_BUTTON_H__

#include <gtk/gtklinkbutton.h>
#include "git-commit.h"

G_BEGIN_DECLS

#define GIT_TYPE_COMMIT_LINK_BUTTON                                     \
  (git_commit_link_button_get_type())
#define GIT_COMMIT_LINK_BUTTON(obj)                                     \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               GIT_TYPE_COMMIT_LINK_BUTTON,             \
                               GitCommitLinkButton))
#define GIT_COMMIT_LINK_BUTTON_CLASS(klass)                             \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            GIT_TYPE_COMMIT_LINK_BUTTON,                \
                            GitCommitLinkButtonClass))
#define GIT_IS_COMMIT_LINK_BUTTON(obj)                                  \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               GIT_TYPE_COMMIT_LINK_BUTTON))
#define GIT_IS_COMMIT_LINK_BUTTON_CLASS(klass)                          \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            GIT_TYPE_COMMIT_LINK_BUTTON))
#define GIT_COMMIT_LINK_BUTTON_GET_CLASS(obj)                           \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              GIT_TYPE_COMMIT_LINK_BUTTON,              \
                              GitCommitLinkButtonClass))

typedef struct _GitCommitLinkButton        GitCommitLinkButton;
typedef struct _GitCommitLinkButtonClass   GitCommitLinkButtonClass;
typedef struct _GitCommitLinkButtonPrivate GitCommitLinkButtonPrivate;

struct _GitCommitLinkButtonClass
{
  GtkLinkButtonClass parent_class;
};

struct _GitCommitLinkButton
{
  GtkLinkButton parent;
  
  GitCommitLinkButtonPrivate *priv;
};

GType git_commit_link_button_get_type (void) G_GNUC_CONST;

GtkWidget *git_commit_link_button_new (GitCommit *commit);

void git_commit_link_button_set_commit (GitCommitLinkButton *lbutton,
					GitCommit *commit);
GitCommit *git_commit_link_button_get_commit (GitCommitLinkButton *lbutton);

G_END_DECLS

#endif /* __GIT_COMMIT_LINK_BUTTON_H__ */

