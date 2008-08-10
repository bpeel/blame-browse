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

#ifndef __GIT_READER_H__
#define __GIT_READER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GIT_TYPE_READER                                                 \
  (git_reader_get_type())
#define GIT_READER(obj)                                                 \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               GIT_TYPE_READER,                         \
                               GitReader))
#define GIT_READER_CLASS(klass)                                         \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            GIT_TYPE_READER,                            \
                            GitReaderClass))
#define GIT_IS_READER(obj)                                              \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               GIT_TYPE_READER))
#define GIT_IS_READER_CLASS(klass)                                      \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            GIT_TYPE_READER))
#define GIT_READER_GET_CLASS(obj)                                       \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              GIT_TYPE_READER,                          \
                              GitReaderClass))

typedef struct _GitReader        GitReader;
typedef struct _GitReaderClass   GitReaderClass;
typedef struct _GitReaderPrivate GitReaderPrivate;

struct _GitReaderClass
{
  GObjectClass parent_class;

  void (* completed) (GitReader *reader, const GError *error);
  gboolean (* line) (GitReader *reader, guint length, const gchar *line);
};

struct _GitReader
{
  GObject parent;

  GitReaderPrivate *priv;
};

GType git_reader_get_type (void) G_GNUC_CONST;

GitReader *git_reader_new (void);

gboolean git_reader_start (GitReader *reader,
			   GError **error,
			   ...);

G_END_DECLS

#endif /* __GIT_READER_H__ */
