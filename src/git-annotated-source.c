#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <glib.h>
#include <stdio.h>

#include "git-annotated-source.h"
#include "git-reader.h"
#include "git-common.h"

static void git_annotated_source_dispose (GObject *object);
static void git_annotated_source_finalize (GObject *object);

static void
git_annotated_source_on_reader_completed (GitReader *reader,
					  const GError *error,
					  GitAnnotatedSource *soure);
static gboolean
git_annotated_source_on_line (GitReader *reader,
			      guint length, const gchar *str,
			      GitAnnotatedSource *soure);

G_DEFINE_TYPE (GitAnnotatedSource, git_annotated_source, G_TYPE_OBJECT);

#define GIT_ANNOTATED_SOURCE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GIT_TYPE_ANNOTATED_SOURCE, \
				GitAnnotatedSourcePrivate))

struct _GitAnnotatedSourcePrivate
{
  GitReader *reader;
  guint completed_handler;
  guint line_handler;
};

enum
  {
    COMPLETED,

    LAST_SIGNAL
  };

static guint client_signals[LAST_SIGNAL];

static void
git_annotated_source_class_init (GitAnnotatedSourceClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = git_annotated_source_dispose;
  gobject_class->finalize = git_annotated_source_finalize;

  client_signals[COMPLETED]
    = g_signal_new ("completed",
		    G_TYPE_FROM_CLASS (gobject_class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GitAnnotatedSourceClass, completed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1,
		    G_TYPE_POINTER);

  g_type_class_add_private (klass, sizeof (GitAnnotatedSourcePrivate));
}

static void
git_annotated_source_init (GitAnnotatedSource *self)
{
  GitAnnotatedSourcePrivate *priv;

  priv = self->priv = GIT_ANNOTATED_SOURCE_GET_PRIVATE (self);

  priv->reader = git_reader_new ();

  priv->completed_handler
    = g_signal_connect (priv->reader, "completed",
			G_CALLBACK (git_annotated_source_on_reader_completed),
			self);

  priv->line_handler
    = g_signal_connect (priv->reader, "line",
			G_CALLBACK (git_annotated_source_on_line),
			self);
}

static void
git_annotated_source_dispose (GObject *object)
{
  GitAnnotatedSource *self = (GitAnnotatedSource *) object;
  GitAnnotatedSourcePrivate *priv = self->priv;

  if (priv->reader)
    {
      g_signal_handler_disconnect (priv->reader, priv->completed_handler);
      g_signal_handler_disconnect (priv->reader, priv->line_handler);
      g_object_unref (priv->reader);
      priv->reader = NULL;
    }

  G_OBJECT_CLASS (git_annotated_source_parent_class)->dispose (object);
}

static void
git_annotated_source_finalize (GObject *object)
{
  GitAnnotatedSource *self = (GitAnnotatedSource *) object;

  G_OBJECT_CLASS (git_annotated_source_parent_class)->finalize (object);
}

GitAnnotatedSource *
git_annotated_source_new (void)
{
  GitAnnotatedSource *self = g_object_new (GIT_TYPE_ANNOTATED_SOURCE, NULL);

  return self;
}

gboolean
git_annotated_source_fetch (GitAnnotatedSource *source,
			    const gchar *filename,
			    const gchar *revision,
			    GError **error)
{
  GitAnnotatedSourcePrivate *priv;

  g_return_val_if_fail (GIT_IS_ANNOTATED_SOURCE (source), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (source->priv->reader != NULL, FALSE);

  priv = source->priv;

  return git_reader_start (priv->reader, error, "blame", "-p",
			   filename, revision, NULL);
}

static void
git_annotated_source_on_reader_completed (GitReader *reader,
					  const GError *error,
					  GitAnnotatedSource *source)
{
  g_signal_emit (source, client_signals[COMPLETED], 0, error, NULL);
}

static gboolean
git_annotated_source_on_line (GitReader *reader,
			      guint length, const gchar *str,
			      GitAnnotatedSource *soure)
{
  printf ("line: \"");

  while (length > 0)
    {
      guchar ch = *str++;

      if (ch == '\n')
	fputs ("\\n", stdout);
      else if (ch == '\t')
	fputs ("\\t", stdout);
      else if (ch < 32 || ch >= 128)
	printf ("\\x%x", ch);
      else
	fputc (ch, stdout);

      length--;
    }

  fputs ("\"\n", stdout);

  return TRUE;
}
