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

#ifndef __GIT_SOURCE_VIEW_H__
#define __GIT_SOURCE_VIEW_H__

#include <gtk/gtkwidget.h>
#include <gtk/gtkadjustment.h>
#include "git-commit.h"

G_BEGIN_DECLS

#define GIT_TYPE_SOURCE_VIEW                                            \
  (git_source_view_get_type())
#define GIT_SOURCE_VIEW(obj)                                            \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               GIT_TYPE_SOURCE_VIEW,                    \
                               GitSourceView))
#define GIT_SOURCE_VIEW_CLASS(klass)                                    \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            GIT_TYPE_SOURCE_VIEW,                       \
                            GitSourceViewClass))
#define GIT_IS_SOURCE_VIEW(obj)                                         \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               GIT_TYPE_SOURCE_VIEW))
#define GIT_IS_SOURCE_VIEW_CLASS(klass)                                 \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            GIT_TYPE_SOURCE_VIEW))
#define GIT_SOURCE_VIEW_GET_CLASS(obj)                                  \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              GIT_TYPE_SOURCE_VIEW,                     \
                              GitSourceViewClass))

typedef struct _GitSourceView        GitSourceView;
typedef struct _GitSourceViewClass   GitSourceViewClass;
typedef struct _GitSourceViewPrivate GitSourceViewPrivate;

struct _GitSourceViewClass
{
  GtkWidgetClass parent_class;

  void (* set_scroll_adjustments) (GtkWidget *widget,
                                   GtkAdjustment *hadjustment,
                                   GtkAdjustment *vadjustment);
  void (* commit_selected) (GitSourceView *source_view,
                            GitCommit *commit);
};

struct _GitSourceView
{
  GtkWidget parent;

  GitSourceViewPrivate *priv;
};

typedef enum {
  GIT_SOURCE_VIEW_READY,
  GIT_SOURCE_VIEW_LOADING,
  GIT_SOURCE_VIEW_ERROR
} GitSourceViewState;

GType git_source_view_get_type (void) G_GNUC_CONST;

GtkWidget *git_source_view_new (void);

void git_source_view_set_file (GitSourceView *sview,
                               const gchar *filename,
                               const gchar *revision);

GitSourceViewState git_source_view_get_state (GitSourceView *sview);
const GError *git_source_view_get_state_error (GitSourceView *sview);

G_END_DECLS

#endif /* __GIT_SOURCE_VIEW_H__ */
