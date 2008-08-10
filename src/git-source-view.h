#ifndef __GIT_SOURCE_VIEW_H__
#define __GIT_SOURCE_VIEW_H__

#include <gtk/gtkwidget.h>

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
};

struct _GitSourceView
{
  GtkWidget parent;

  GitSourceViewPrivate *priv;
};

GType git_source_view_get_type (void) G_GNUC_CONST;

GtkWidget *git_source_view_new (void);

void git_source_view_set_file (GitSourceView *sview,
			       const gchar *filename,
			       const gchar *revision);

G_END_DECLS

#endif /* __GIT_SOURCE_VIEW_H__ */
