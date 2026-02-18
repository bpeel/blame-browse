/* This file is part of blame-browse.
 * Copyright (C) 2008, 2026  Neil Roberts  <bpeeluk@yahoo.co.uk>
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

#include "git-hash-view.h"

#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "git-annotated-source.h"
#include "git-marshal.h"
#include "git-common.h"
#include "git-enum-types.h"

static void git_hash_view_dispose (GObject *object);
static void git_hash_view_snapshot (GtkWidget *widget,
                                    GtkSnapshot *snapshot);
static void
git_hash_view_motion_cb (GtkEventControllerMotion *controller,
                         double x,
                         double y,
                         GitHashView *hview);
static void git_hash_view_click_released_cb (GtkGestureClick *self,
                                             gint n_press,
                                             gdouble x,
                                             gdouble y,
                                             gpointer user_data);
static GtkSizeRequestMode git_hash_view_get_request_mode (GtkWidget *widget);
static void git_hash_view_measure (GtkWidget *widget,
                                   GtkOrientation orientation,
                                   int for_size,
                                   int* minimum,
                                   int* natural,
                                   int* minimum_baseline,
                                   int* natural_baseline);
static void git_hash_view_get_property (GObject *object, guint property_id,
                                        GValue *value, GParamSpec *pspec);
static void git_hash_view_set_property (GObject *object, guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec);

static gboolean git_hash_view_query_tooltip (GtkWidget *widget,
                                             gint x, gint y,
                                             gboolean keyboard_tooltip,
                                             GtkTooltip *tooltip);

typedef struct
{
  GtkTextView *text_view;
  GitAnnotatedSource *source;

  GdkCursor *hand_cursor;
  gboolean hand_cursor_set;

  guint adjustment_handler;
  guint adjustment_value_handler;
  GtkAdjustment *text_view_adjustment;
} GitHashViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GitHashView,
                            git_hash_view,
                            GTK_TYPE_WIDGET);

enum
  {
    COMMIT_SELECTED,

    LAST_SIGNAL
  };

enum
  {
    PROP_0,

    PROP_TEXT_VIEW,
    PROP_SOURCE,
  };

static guint client_signals[LAST_SIGNAL];

/* Number of digits of the commit hash to show */
#define GIT_HASH_VIEW_COMMIT_HASH_LENGTH 6

static void
git_hash_view_class_init (GitHashViewClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
  GParamSpec *pspec;

  gobject_class->dispose = git_hash_view_dispose;
  gobject_class->get_property = git_hash_view_get_property;
  gobject_class->set_property = git_hash_view_set_property;

  widget_class->snapshot = git_hash_view_snapshot;
  widget_class->query_tooltip = git_hash_view_query_tooltip;
  widget_class->get_request_mode = git_hash_view_get_request_mode;
  widget_class->measure = git_hash_view_measure;

  pspec = g_param_spec_object ("text-view",
                               "Text view",
                               "The GtkTextView to track in order to match the "
                               "commit hashes against the lines displayed in "
                               "the view.",
                               GTK_TYPE_TEXT_VIEW,
                               G_PARAM_READABLE | G_PARAM_WRITABLE);
  g_object_class_install_property (gobject_class, PROP_TEXT_VIEW, pspec);

  pspec = g_param_spec_object ("source",
                               "Source",
                               "The annotated source code to fetch the "
                               "commit hashes from.",
                               GIT_TYPE_ANNOTATED_SOURCE,
                               G_PARAM_READABLE | G_PARAM_WRITABLE);
  g_object_class_install_property (gobject_class, PROP_SOURCE, pspec);

  client_signals[COMMIT_SELECTED]
    = g_signal_new ("commit-selected",
                    G_OBJECT_CLASS_TYPE (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GitHashViewClass, commit_selected),
                    NULL, NULL,
                    _git_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1,
                    GIT_TYPE_COMMIT);
}

static void
git_hash_view_init (GitHashView *hview)
{
  g_object_set (hview, "has-tooltip", TRUE, NULL);

  gtk_widget_add_css_class (GTK_WIDGET (hview), "view");
  gtk_widget_add_css_class (GTK_WIDGET (hview), "monospace");

  GtkEventController *motion_controller = gtk_event_controller_motion_new ();
  g_signal_connect (motion_controller, "motion",
                    G_CALLBACK (git_hash_view_motion_cb), hview);
  gtk_widget_add_controller (GTK_WIDGET (hview), motion_controller);

  GtkGesture *gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture),
                                 GDK_BUTTON_PRIMARY);
  g_signal_connect (gesture,
                    "released",
                    G_CALLBACK (git_hash_view_click_released_cb),
                    hview);
  GtkEventController *event_controller = GTK_EVENT_CONTROLLER (gesture);
  gtk_event_controller_set_propagation_phase (event_controller,
                                              GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (hview), event_controller);
}

static void
git_hash_view_unref_text_view_adjustment (GitHashView *hview)
{
  GitHashViewPrivate *priv = git_hash_view_get_instance_private (hview);

  if (priv->text_view_adjustment)
    {
      g_signal_handler_disconnect (priv->text_view_adjustment,
                                   priv->adjustment_value_handler);
      g_object_unref (priv->text_view_adjustment);
      priv->text_view_adjustment = NULL;
    }
}

static void
git_hash_view_unref_text_view (GitHashView *hview)
{
  GitHashViewPrivate *priv = git_hash_view_get_instance_private (hview);

  if (priv->text_view)
    {
      git_hash_view_unref_text_view_adjustment (hview);
      g_signal_handler_disconnect (priv->text_view,
                                   priv->adjustment_handler);
      g_object_unref (priv->text_view);
      priv->text_view = NULL;
    }
}

static void
git_hash_view_unref_source (GitHashView *hview)
{
  GitHashViewPrivate *priv = git_hash_view_get_instance_private (hview);

  if (priv->source)
    {
      g_object_unref (priv->source);
      priv->source = NULL;
    }
}

static void
git_hash_view_dispose (GObject *object)
{
  GitHashView *hview = (GitHashView *) object;

  git_hash_view_unref_text_view (hview);
  git_hash_view_unref_source (hview);

  G_OBJECT_CLASS (git_hash_view_parent_class)->dispose (object);
}

static void
git_hash_view_set_text_for_commit (PangoLayout *layout, GitCommit *commit)
{
  const gchar *hash = git_commit_get_hash (commit), *p;
  int len = strlen (hash);

  /* If the hash is all zeroes then it represents lines in the working
     copy that have not been committed */
  for (p = hash; *p == '0'; p++);
  if (p - hash == len && len == GIT_COMMIT_HASH_LENGTH)
    pango_layout_set_markup (layout, "<i>WIP</i>", -1);
  else
    {
      if (len > GIT_HASH_VIEW_COMMIT_HASH_LENGTH)
        len = GIT_HASH_VIEW_COMMIT_HASH_LENGTH;

      pango_layout_set_text (layout, hash, len);
      pango_layout_set_attributes (layout, NULL);
    }
}

static void
git_hash_view_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  GitHashView *hview = (GitHashView *) widget;
  GitHashViewPrivate *priv = git_hash_view_get_instance_private (hview);
  int width = gtk_widget_get_width (widget);
  int height = gtk_widget_get_height (widget);

  if (priv->text_view == NULL || priv->source == NULL)
    return;

  GtkTextBuffer *text_buffer = gtk_text_view_get_buffer (priv->text_view);

  if (text_buffer == NULL)
    return;

  PangoLayout *layout = gtk_widget_create_pango_layout (widget, NULL);

  gsize n_lines = git_annotated_source_get_n_lines (priv->source);

  gint buffer_top_y;
  gtk_text_view_window_to_buffer_coords (priv->text_view,
                                         GTK_TEXT_WINDOW_TEXT,
                                         0, 0, /* window_x/y */
                                         NULL, /* buffer_x */
                                         &buffer_top_y);

  GtkTextIter iter;
  gtk_text_view_get_line_at_y (priv->text_view,
                               &iter,
                               buffer_top_y,
                               NULL /* line_top */);

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT (0, 0, width, height));

  while (TRUE)
    {
      int line_buffer_y, line_height;
      gtk_text_view_get_line_yrange (priv->text_view,
                                     &iter,
                                     &line_buffer_y, &line_height);

      int window_x, window_y;
      gtk_text_view_buffer_to_window_coords (priv->text_view,
                                             GTK_TEXT_WINDOW_TEXT,
                                             0, line_buffer_y,
                                             &window_x, &window_y);

      if (window_y >= height)
        break;

      int line_num = gtk_text_iter_get_line (&iter);

      if (line_num < 0 || line_num >= n_lines)
        break;

      const GitAnnotatedSourceLine *line
        = git_annotated_source_get_line (priv->source, line_num);
      GdkRGBA color;

      git_commit_get_color (line->commit, &color);

      gtk_snapshot_append_color (snapshot,
                                 &color, &GRAPHENE_RECT_INIT (0, window_y,
                                                              width,
                                                              line_height));

      /* Invert the color so that the text is guaranteed to be a
         different (albeit clashing) colour */
      color.red = 1.0 - color.red;
      color.green = 1.0 - color.green;
      color.blue = 1.0 - color.blue;

      git_hash_view_set_text_for_commit (layout, line->commit);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, window_y));
      gtk_snapshot_append_layout (snapshot,
                                  layout,
                                  &color);
      gtk_snapshot_restore (snapshot);

      if (!gtk_text_view_forward_display_line (priv->text_view, &iter))
        break;
    }

  gtk_snapshot_pop (snapshot);

  g_object_unref (layout);
}

static GitCommit *
get_iter_and_commit_at_y (GitHashView *hview, int window_y, GtkTextIter *iter)
{
  GitHashViewPrivate *priv = git_hash_view_get_instance_private (hview);

  if (priv->source == NULL || priv->text_view == NULL)
    return NULL;

  int buffer_y;

  gtk_text_view_window_to_buffer_coords (priv->text_view,
                                         GTK_TEXT_WINDOW_TEXT,
                                         0, window_y,
                                         NULL, /* buffer_x */
                                         &buffer_y);

  gtk_text_view_get_line_at_y (priv->text_view,
                               iter,
                               buffer_y,
                               NULL /* line_top */);

  int line_num = gtk_text_iter_get_line (iter);

  if (line_num < 0
      || line_num >= git_annotated_source_get_n_lines (priv->source))
    return NULL;

  return git_annotated_source_get_line (priv->source, line_num)->commit;
}

static gboolean
git_hash_view_query_tooltip (GtkWidget *widget,
                             gint x, gint y,
                             gboolean keyboard_tooltip,
                             GtkTooltip *tooltip)
{
  GitHashView *hview = (GitHashView *) widget;
  GitHashViewPrivate *priv = git_hash_view_get_instance_private (hview);

  GtkTextIter iter;
  GitCommit *commit = get_iter_and_commit_at_y (hview, y, &iter);

  if (commit == NULL)
    return FALSE;

  if (priv->source == NULL || priv->text_view == NULL)
    return FALSE;

  gboolean ret = TRUE;

  GString *markup = g_string_new ("");
  const char *part;

  if ((part = git_commit_get_prop (commit, "author")))
    {
      char *part_markup = g_markup_printf_escaped ("<b>%s</b>", part);
      g_string_append (markup, part_markup);
      g_free (part_markup);
    }
  if ((part = git_commit_get_prop (commit, "author-mail")))
    {
      if (markup->len > 0)
        g_string_append_c (markup, ' ');
      char *part_markup = g_markup_escape_text (part, -1);
      g_string_append (markup, part_markup);
      g_free (part_markup);
    }
  if ((part = git_commit_get_prop (commit, "author-time")))
    {
      long unix_timestamp;
      char *tail;

      errno = 0;
      unix_timestamp = strtol (part, &tail, 10);

      if (errno == 0 && *tail == '\0')
        {
          GDateTime *dt = g_date_time_new_from_unix_local (unix_timestamp);

          gchar *display_time = git_format_time_for_display (dt);

          if (display_time)
            {
              if (markup->len > 0)
                g_string_append_c (markup, '\n');
              char *part_markup = g_markup_escape_text (display_time, -1);
              g_free (display_time);
              g_string_append (markup, part_markup);
              g_free (part_markup);
            }

          g_date_time_unref (dt);
        }
    }
  if ((part = git_commit_get_prop (commit, "summary")))
    {
      gchar *stripped_part;

      if (markup->len > 0)
        g_string_append_c (markup, '\n');

      while (*part && isspace (*part))
        part++;
      stripped_part = g_strdup (part);
      g_strchomp (stripped_part);

      char *part_markup = g_markup_printf_escaped ("<i>%s</i>", stripped_part);
      g_free (stripped_part);
      g_string_append (markup, part_markup);
      g_free (part_markup);
    }

  if (markup->len > 0)
    {
      GdkRectangle buffer_location;
      gtk_text_view_get_iter_location (priv->text_view,
                                       &iter,
                                       &buffer_location);

      int window_x, window_y;
      gtk_text_view_buffer_to_window_coords (priv->text_view,
                                             GTK_TEXT_WINDOW_TEXT,
                                             0, buffer_location.y,
                                             &window_x, &window_y);

      GdkRectangle tip_area;
      tip_area.x = 0;
      tip_area.y = MAX (0, window_y);
      tip_area.width = gtk_widget_get_width (GTK_WIDGET (hview));
      tip_area.height = MIN (gtk_widget_get_height (GTK_WIDGET (hview))
                             - tip_area.y,
                             buffer_location.height);
      gtk_tooltip_set_markup (tooltip, markup->str);
      gtk_tooltip_set_tip_area (tooltip, &tip_area);
    }
  else
    ret = FALSE;

  g_string_free (markup, TRUE);

  return ret;
}

static void
git_hash_view_get_property (GObject *object, guint property_id,
                            GValue *value, GParamSpec *pspec)
{
  GitHashView *hview = (GitHashView *) object;
  GitHashViewPrivate *priv = git_hash_view_get_instance_private (hview);

  switch (property_id)
    {
    case PROP_TEXT_VIEW:
      g_value_set_object (value, priv->text_view);
      break;
    case PROP_SOURCE:
      g_value_set_object (value, priv->source);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
git_hash_view_set_property (GObject *object, guint property_id,
                            const GValue *value, GParamSpec *pspec)
{
  GitHashView *hview = (GitHashView *) object;

  switch (property_id)
    {
    case PROP_TEXT_VIEW:
      git_hash_view_set_text_view (hview, g_value_get_object (value));
      break;
    case PROP_SOURCE:
      git_hash_view_set_source (hview, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

GtkWidget *
git_hash_view_new (GtkTextView *text_view)
{
  GtkWidget *widget = g_object_new (GIT_TYPE_HASH_VIEW,
                                    "text-view", text_view,
                                    NULL);

  return widget;
}

static void
git_hash_view_motion_cb (GtkEventControllerMotion *controller,
                         double x,
                         double y,
                         GitHashView *hview)
{
  GitHashViewPrivate *priv = git_hash_view_get_instance_private (hview);

  GtkTextIter iter;
  GitCommit *commit = get_iter_and_commit_at_y (hview, y, &iter);

  /* Show the hand cursor when the pointer is over a commit hash */
  if (commit)
    {
      if (!priv->hand_cursor_set)
        {
          /* Create the hand cursor if we haven't already got
             one. This will be unref'd in the dispose handler */
          if (priv->hand_cursor == NULL)
            priv->hand_cursor = gdk_cursor_new_from_name ("pointer", NULL);
          gtk_widget_set_cursor (GTK_WIDGET (hview), priv->hand_cursor);

          priv->hand_cursor_set = TRUE;
        }
    }
  else if (priv->hand_cursor_set)
    {
      gtk_widget_set_cursor (GTK_WIDGET (hview), NULL);
      priv->hand_cursor_set = FALSE;
    }
}

static void
git_hash_view_click_released_cb (GtkGestureClick *self,
                                 gint n_press,
                                 gdouble x,
                                 gdouble y,
                                 gpointer user_data)
{
  GitHashView *hview = user_data;

  GtkTextIter iter;
  GitCommit *commit = get_iter_and_commit_at_y (hview, y, &iter);

  if (commit)
    {
      g_signal_emit (hview,
                     client_signals[COMMIT_SELECTED],
                     0,
                     commit);
    }
}

static GtkSizeRequestMode
git_hash_view_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
git_hash_view_measure (GtkWidget *widget,
                       GtkOrientation orientation,
                       int for_size,
                       int* minimum,
                       int* natural,
                       int* minimum_baseline,
                       int* natural_baseline)
{
  switch (orientation)
    {
    case GTK_ORIENTATION_VERTICAL:
      *minimum = 10;
      *natural = 10;
      break;
    case GTK_ORIENTATION_HORIZONTAL:
      {
        PangoLayout *layout = gtk_widget_create_pango_layout (widget, NULL);
        char dummy_git_hash[GIT_HASH_VIEW_COMMIT_HASH_LENGTH];

        memset(dummy_git_hash, '0', sizeof dummy_git_hash);
        pango_layout_set_text (layout,
                               dummy_git_hash,
                               GIT_HASH_VIEW_COMMIT_HASH_LENGTH);

        PangoRectangle extents;
        pango_layout_get_extents (layout,
                                  NULL, /* ink_rect */
                                  &extents /* logical_rect */);

        gint pixel_width = (extents.width + PANGO_SCALE - 1) / PANGO_SCALE;

        *minimum = pixel_width;
        *natural = pixel_width;

        g_object_unref (layout);
      }
      break;
    }

  *minimum_baseline = -1;
  *natural_baseline = -1;
}

static void
git_source_view_on_scrolled (GtkAdjustment *adjustment,
                             GitHashView *hview)
{
  if (gtk_widget_get_realized (GTK_WIDGET (hview)))
    gtk_widget_queue_draw (GTK_WIDGET (hview));
}

static void
update_text_view_adjustment (GitHashView *hview)
{
  GitHashViewPrivate *priv = git_hash_view_get_instance_private (hview);

  git_hash_view_unref_text_view_adjustment (hview);

  GtkAdjustment *adjustment
    = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (priv->text_view));

  if (adjustment)
    {
      priv->text_view_adjustment = g_object_ref (adjustment);

      priv->adjustment_value_handler =
        g_signal_connect (adjustment,
                          "value-changed",
                          G_CALLBACK (git_source_view_on_scrolled),
                          hview);
    }
}

static void
git_hash_view_on_adjustment_changed (GtkTextView *text_view,
                                     GParamSpec *pspec,
                                     GitHashView *hview)
{
  update_text_view_adjustment (hview);
}

void
git_hash_view_set_text_view (GitHashView *hview,
                             GtkTextView *text_view)
{
  GitHashViewPrivate *priv = git_hash_view_get_instance_private (hview);

  git_hash_view_unref_text_view (hview);

  if (text_view)
    {
      priv->text_view = g_object_ref (text_view);

      priv->adjustment_handler =
        g_signal_connect (text_view,
                          "notify::vadjustment",
                          G_CALLBACK (git_hash_view_on_adjustment_changed),
                          hview);

      update_text_view_adjustment (hview);
    }

  if (gtk_widget_get_realized (GTK_WIDGET (hview)))
      gtk_widget_queue_draw (GTK_WIDGET (hview));
}

void git_hash_view_set_source (GitHashView *hview,
                               GitAnnotatedSource *source)
{
  GitHashViewPrivate *priv = git_hash_view_get_instance_private (hview);

  git_hash_view_unref_source (hview);

  if (source)
    priv->source = g_object_ref (source);

  if (gtk_widget_get_realized (GTK_WIDGET (hview)))
      gtk_widget_queue_draw (GTK_WIDGET (hview));
}
