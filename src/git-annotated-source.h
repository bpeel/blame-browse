#ifndef __GIT_ANNOTATED_SOURCE_H__
#define __GIT_ANNOTATED_SOURCE_H__

#include <glib-object.h>
#include "git-commit.h"

G_BEGIN_DECLS

#define GIT_TYPE_ANNOTATED_SOURCE                                       \
  (git_annotated_source_get_type())
#define GIT_ANNOTATED_SOURCE(obj)                                       \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               GIT_TYPE_ANNOTATED_SOURCE,               \
                               GitAnnotatedSource))
#define GIT_ANNOTATED_SOURCE_CLASS(klass)                               \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            GIT_TYPE_ANNOTATED_SOURCE,                  \
                            GitAnnotatedSourceClass))
#define GIT_IS_ANNOTATED_SOURCE(obj)                                    \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               GIT_TYPE_ANNOTATED_SOURCE))
#define GIT_IS_ANNOTATED_SOURCE_CLASS(klass)                            \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            GIT_TYPE_ANNOTATED_SOURCE))
#define GIT_ANNOTATED_SOURCE_GET_CLASS(obj)                             \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              GIT_TYPE_ANNOTATED_SOURCE,                \
                              GitAnnotatedSourceClass))

typedef struct _GitAnnotatedSource        GitAnnotatedSource;
typedef struct _GitAnnotatedSourceClass   GitAnnotatedSourceClass;
typedef struct _GitAnnotatedSourcePrivate GitAnnotatedSourcePrivate;
typedef struct _GitAnnotatedSourceLine    GitAnnotatedSourceLine;

struct _GitAnnotatedSourceClass
{
  GObjectClass parent_class;

  void (* completed) (GitAnnotatedSource *source, const GError *error);
};

struct _GitAnnotatedSource
{
  GObject parent;

  GitAnnotatedSourcePrivate *priv;
};

struct _GitAnnotatedSourceLine
{
  GitCommit *commit;
  guint orig_line, final_line;
  gchar *text;
};

GType git_annotated_source_get_type (void) G_GNUC_CONST;

GitAnnotatedSource *git_annotated_source_new (void);
gboolean git_annotated_source_fetch (GitAnnotatedSource *source,
				     const gchar *filename,
				     const gchar *revision,
				     GError **error);

gsize git_annotated_source_get_n_lines (GitAnnotatedSource *source);

const GitAnnotatedSourceLine *
git_annotated_source_get_line (GitAnnotatedSource *source, gsize line_num);

G_END_DECLS

#endif /* __GIT_ANNOTATED_SOURCE_H__ */
