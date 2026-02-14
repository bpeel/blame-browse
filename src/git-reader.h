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

#define GIT_TYPE_READER git_reader_get_type()

G_DECLARE_DERIVABLE_TYPE (GitReader,
                          git_reader,
                          GIT,
                          READER,
                          GObject);

struct _GitReaderClass
{
  GObjectClass parent_class;

  void (* completed) (GitReader *reader, const GError *error);
  gboolean (* line) (GitReader *reader, guint length, const gchar *line);
};

GitReader *git_reader_new (void);

gboolean git_reader_start (GitReader *reader,
                           const gchar *working_directory,
                           GError **error,
                           ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* __GIT_READER_H__ */
