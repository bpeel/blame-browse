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

#ifndef __GIT_COMMON_H__
#define __GIT_COMMON_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#define GIT_ERROR (git_error_quark ())

typedef enum {
  GIT_ERROR_EXIT_STATUS,
  GIT_ERROR_PARSE_ERROR,
  GIT_ERROR_NO_REPO
} GitError;

GQuark git_error_quark (void);

gboolean _git_boolean_continue_accumulator (GSignalInvocationHint *ihint,
                                            GValue *return_accu,
                                            const GValue *handler_return,
                                            gpointer data);
gchar *git_format_time_for_display (GTimeVal *time_);

gboolean git_find_repo (const gchar *full_filename, gchar **repo,
                        gchar **relative_filename);

#endif /* __GIT_COMMON_H__ */
