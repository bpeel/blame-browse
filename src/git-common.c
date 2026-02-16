/* This file is part of blame-browse.
 * Copyright (C) 2008  Neil Roberts  <bpeeluk@yahoo.co.uk>
 * Parts of this file are
 * Copyright (C) 2008  Emmanuele Bassi  <ebassi@gnome.org>
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

#include "config.h"

#include "git-common.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

GQuark
git_error_quark (void)
{
  return g_quark_from_static_string ("git-error-quark");
}

gboolean
_git_boolean_continue_accumulator (GSignalInvocationHint *ihint,
                                   GValue *return_accu,
                                   const GValue *handler_return,
                                   gpointer data)
{
  gboolean continue_emission;

  continue_emission = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, continue_emission);

  return continue_emission;
}

GFile *
git_find_repo (GFile *filename)
{
  GFile *repo = g_object_ref (filename);

  while (true)
    {
      GFile *parent = g_file_get_parent (repo);

      g_object_unref (repo);

      if (parent == NULL)
        return NULL;

      repo = parent;

      GFile *dot_git = g_file_get_child (repo, ".git");

      gboolean found = g_file_query_exists (dot_git, NULL);

      g_object_unref (dot_git);

      if (found)
        return repo;
    }
}

/* This function is stolen from Tweet and then later adapted to use
   GDateTime */
gchar *
git_format_time_for_display (GDateTime *dt)
{
  GDateTime *now = g_date_time_new_now_local ();

  if (now == NULL)
    return NULL;

  gchar *retval = NULL;
  GTimeSpan diff = g_date_time_difference (now, dt);

  if (diff < G_TIME_SPAN_MINUTE)
    retval = g_strdup (_("Less than a minute ago"));
  else
    {
      gint mins_diff = diff / G_TIME_SPAN_MINUTE;

      if (mins_diff < 60)
        {
          retval = g_strdup_printf (ngettext ("About a minute ago",
                                              "About %d minutes ago",
                                              mins_diff),
                                    mins_diff);
        }
      else if (mins_diff < 360)
        {
          gint hours_diff = mins_diff / 60;

          retval = g_strdup_printf (ngettext ("About an hour ago",
                                              "About %d hours ago",
                                              hours_diff),
                                    hours_diff);
        }
      else
        {
          GDate d1, d2;
          gint days_diff;

          g_date_set_time_t (&d1, g_date_time_to_unix (dt));
          g_date_set_time_t (&d2, g_date_time_to_unix (now));

          days_diff = g_date_days_between (&d1, &d2);

          const gchar *format = NULL;

          if (days_diff == 0)
            format = _("Today at %H:%M");
          else if (days_diff == 1)
            format = _("Yesterday at %H:%M");
          else if (days_diff > 1 && days_diff < 7)
            format = _("Last %A at %H:%M"); /* day of the week */
          else
            format = _("%x at %H:%M"); /* any other date */

          retval = g_date_time_format (dt, format);
        }
    }

  g_date_time_unref (now);

  return retval;
}
