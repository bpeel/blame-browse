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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "git-common.h"

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

gboolean
git_find_repo (const gchar *filename, gchar **repo,
               gchar **relative_filename)
{
  int len;
  gchar *full_filename, *full_filename_copy;
  gboolean ret = FALSE;

  if (*filename == 0)
    return FALSE;

  /* Convert the path to an absolute path */
  if (g_path_is_absolute (filename))
    full_filename = g_strdup (filename);
  else
    {
      gchar *current_dir = g_get_current_dir ();
      full_filename = g_build_filename (current_dir, filename, NULL);
      g_free (current_dir);
    }

  len = strlen (full_filename);
  full_filename_copy = g_malloc (len + strlen (G_DIR_SEPARATOR_S)
                                 + strlen (".git") + 1);

  memcpy (full_filename_copy, full_filename, len);

  while (TRUE)
    {
      while (len > 0 && G_IS_DIR_SEPARATOR (full_filename[len - 1]))
        len--;
      while (len > 0 && !G_IS_DIR_SEPARATOR (full_filename[len - 1]))
        len--;
      while (len > 0 && G_IS_DIR_SEPARATOR (full_filename[len - 1]))
        len--;

      if (len == 0)
        break;

      strcpy (full_filename_copy + len, G_DIR_SEPARATOR_S ".git");

      if (g_file_test (full_filename_copy, G_FILE_TEST_EXISTS))
        {
          if (repo)
            {
              *repo = g_malloc (len + 1);
              memcpy (*repo, full_filename, len);
              (*repo)[len] = '\0';
            }
          if (relative_filename)
            {
              const gchar *p = full_filename + len;

              while (G_IS_DIR_SEPARATOR (*p))
                p++;

              *relative_filename = g_strdup (p);
            }

          ret = TRUE;

          break;
        }
    }

  g_free (full_filename_copy);
  g_free (full_filename);

  return ret;
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
