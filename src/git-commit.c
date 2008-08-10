#include <glib-object.h>

#include "git-commit.h"

#define GIT_COMMIT_DEFAULT_HASH "0000000000000000000000000000000000000000"

static void git_commit_finalize (GObject *object);
static void git_commit_set_property (GObject *object, guint property_id,
				     const GValue *value, GParamSpec *pspec);
static void git_commit_get_property (GObject *object, guint property_id,
				     GValue *value, GParamSpec *pspec);

G_DEFINE_TYPE (GitCommit, git_commit, G_TYPE_OBJECT);

#define GIT_COMMIT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GIT_TYPE_COMMIT, \
				GitCommitPrivate))

struct _GitCommitPrivate
{
  gchar *hash;
  GHashTable *props;
};

enum
  {
    PROP_0,

    PROP_HASH
  };

static void
git_commit_class_init (GitCommitClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GParamSpec *pspec;

  gobject_class->finalize = git_commit_finalize;
  gobject_class->set_property = git_commit_set_property;
  gobject_class->get_property = git_commit_get_property;

  g_type_class_add_private (klass, sizeof (GitCommitPrivate));

  pspec = g_param_spec_string ("hash",
			       "Commit hash",
			       "The SHA1 hash used to name the commit",
			       GIT_COMMIT_DEFAULT_HASH,
			       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READABLE
			       | G_PARAM_WRITABLE);
  g_object_class_install_property (gobject_class, PROP_HASH, pspec);
}

static void
git_commit_init (GitCommit *self)
{
  GitCommitPrivate *priv;

  priv = self->priv = GIT_COMMIT_GET_PRIVATE (self);

  priv->props = g_hash_table_new_full (g_str_hash, g_str_equal,
				       g_free, g_free);
}

static void
git_commit_finalize (GObject *object)
{
  GitCommit *self = (GitCommit *) object;
  GitCommitPrivate *priv = self->priv;

  if (priv->hash)
    g_free (priv->hash);
  g_hash_table_destroy (priv->props);

  G_OBJECT_CLASS (git_commit_parent_class)->finalize (object);
}

static void
git_commit_set_property (GObject *object, guint property_id,
			 const GValue *value, GParamSpec *pspec)
{
  GitCommit *commit = (GitCommit *) object;
  GitCommitPrivate *priv = commit->priv;

  switch (property_id)
    {
    case PROP_HASH:
      if (priv->hash)
	g_free (priv->hash);
      priv->hash = g_strdup (g_value_get_string (value));
      break;
    }
}

static void
git_commit_get_property (GObject *object, guint property_id,
			 GValue *value, GParamSpec *pspec)
{
  GitCommit *commit = (GitCommit *) object;
  GitCommitPrivate *priv = commit->priv;

  switch (property_id)
    {
    case PROP_HASH:
      g_value_set_string (value, priv->hash);
      break;
    }
}

GitCommit *
git_commit_new (const gchar *hash)
{
  GitCommit *self = g_object_new (GIT_TYPE_COMMIT,
				  "hash", hash,
				  NULL);

  return self;
}

const gchar *
git_commit_get_hash (GitCommit *commit)
{
  g_return_val_if_fail (GIT_IS_COMMIT (commit), NULL);

  return commit->priv->hash;
}

void
git_commit_set_prop (GitCommit *commit, const gchar *prop_name,
		     const gchar *value)
{
  g_return_if_fail (GIT_IS_COMMIT (commit));

  g_hash_table_insert (commit->priv->props,
		       g_strdup (prop_name),
		       g_strdup (value));
}

const gchar *
git_commit_get_prop (GitCommit *commit, const gchar *prop_name)
{
  g_return_val_if_fail (GIT_IS_COMMIT (commit), NULL);

  return g_hash_table_lookup (commit->priv->props, prop_name);
}
