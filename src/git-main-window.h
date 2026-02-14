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

#ifndef __GIT_MAIN_WINDOW_H__
#define __GIT_MAIN_WINDOW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GIT_TYPE_MAIN_WINDOW git_main_window_get_type ()

G_DECLARE_FINAL_TYPE (GitMainWindow,
                      git_main_window,
                      GIT,
                      MAIN_WINDOW,
                      GtkWindow);

GtkWidget *git_main_window_new (void);

void git_main_window_set_file (GitMainWindow *main_window,
                               const gchar *filename,
                               const gchar *revision);

G_END_DECLS

#endif /* __GIT_MAIN_WINDOW_H__ */
