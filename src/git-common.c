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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <string.h>

#include "git-common.h"
#include "intl.h"

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

/* This function is stolen from Tweet */
gchar *
git_format_time_for_display (GTimeVal *time_)
{
  GTimeVal now;
  struct tm tm_mtime;
  const gchar *format = NULL;
  gchar *locale_format = NULL;
  gchar buf[256];
  gchar *retval = NULL;
  gint secs_diff;

  g_return_val_if_fail (time_->tv_usec >= 0
			&& time_->tv_usec < G_USEC_PER_SEC, NULL);

  g_get_current_time (&now);

#ifdef HAVE_LOCALTIME_R
  localtime_r ((time_t *) &(time_->tv_sec), &tm_mtime);
#else
  {
    struct tm *ptm = localtime ((time_t *) &(time_->tv_usec));

    if (!ptm)
      {
        g_warning ("ptm != NULL failed");
        return NULL;
      }
    else
      memcpy ((void *) &tm_mtime, (void *) ptm, sizeof (struct tm));
  }
#endif /* HAVE_LOCALTIME_R */

  secs_diff  = now.tv_sec - time_->tv_sec;

  /* within the hour */
  if (secs_diff < 60)
    retval = g_strdup (_("Less than a minute ago"));
  else
    {
      gint mins_diff = secs_diff / 60;

      if (mins_diff < 60)
        retval = g_strdup_printf (ngettext ("About a minute ago",
                                            "About %d minutes ago",
                                            mins_diff),
                                  mins_diff);
      else if (mins_diff < 360)
        {
          gint hours_diff = mins_diff / 60;

          retval = g_strdup_printf (ngettext ("About an hour ago",
                                              "About %d hours ago",
                                              hours_diff),
                                    hours_diff);
        }
    }

  if (retval)
    return retval;
  else
    {
      GDate d1, d2;
      gint days_diff;

      g_date_set_time_t (&d1, now.tv_sec);
      g_date_set_time_t (&d2, time_->tv_sec);

      days_diff = g_date_get_julian (&d1) - g_date_get_julian (&d2);

      if (days_diff == 0)
        format = _("Today at %H:%M");
      else if (days_diff == 1)
        format = _("Yesterday at %H:%M");
      else
        {
          if (days_diff > 1 && days_diff < 7)
            format = _("Last %A at %H:%M"); /* day of the week */
          else
            format = _("%x at %H:%M"); /* any other date */
        }
    }

  locale_format = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);

  if (strftime (buf, sizeof (buf), locale_format, &tm_mtime) != 0)
    retval = g_locale_to_utf8 (buf, -1, NULL, NULL, NULL);
  else
    retval = g_strdup (_("Unknown"));

  g_free (locale_format);

  return retval;
}
