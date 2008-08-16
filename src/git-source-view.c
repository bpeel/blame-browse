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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtkwidget.h>
#include <gtk/gtktooltip.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "git-source-view.h"
#include "git-annotated-source.h"
#include "git-marshal.h"
#include "git-common.h"
#include "git-enum-types.h"

static void git_source_view_dispose (GObject *object);
static void git_source_view_realize (GtkWidget *widget);
static gboolean git_source_view_expose_event (GtkWidget *widget,
					      GdkEventExpose *event);
static void git_source_view_set_scroll_adjustments (GtkWidget *widget,
						    GtkAdjustment *hadjustment,
						    GtkAdjustment *vadjustment);
static void git_source_view_size_allocate (GtkWidget *widget,
					   GtkAllocation *allocation);
static gboolean git_source_view_motion_notify_event (GtkWidget *widget,
						     GdkEventMotion *event);
static gboolean git_source_view_button_release_event (GtkWidget *widget,
						      GdkEventButton *event);
static void git_source_view_get_property (GObject *object, guint property_id,
					  GValue *value, GParamSpec *pspec);

static gboolean git_source_view_query_tooltip (GtkWidget *widget,
					       gint x, gint y,
					       gboolean keyboard_tooltip,
					       GtkTooltip *tooltip);

G_DEFINE_TYPE (GitSourceView, git_source_view, GTK_TYPE_WIDGET);

#define GIT_SOURCE_VIEW_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GIT_TYPE_SOURCE_VIEW, \
				GitSourceViewPrivate))

struct _GitSourceViewPrivate
{
  GitAnnotatedSource *paint_source, *load_source;
  guint loading_completed_handler;

  guint line_height, max_line_width, max_hash_length;

  GtkAdjustment *hadjustment, *vadjustment;
  guint hadjustment_value_changed_handler;
  guint vadjustment_value_changed_handler;
  guint hadjustment_changed_handler;
  guint vadjustment_changed_handler;

  GitSourceViewState state;
  GError *state_error;
  
  gint x_offset, y_offset;

  GdkCursor *hand_cursor;
  gboolean hand_cursor_set;
};

enum
  {
    SET_SCROLL_ADJUSTMENTS,
    COMMIT_SELECTED,

    LAST_SIGNAL
  };

enum
  {
    PROP_0,

    PROP_STATE
  };

static guint client_signals[LAST_SIGNAL];

/* Number of digits of the commit hash to show */
#define GIT_SOURCE_VIEW_COMMIT_HASH_LENGTH 6

#define GIT_SOURCE_VIEW_GAP 3

static void
git_source_view_class_init (GitSourceViewClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
  GParamSpec *pspec;

  gobject_class->dispose = git_source_view_dispose;
  gobject_class->get_property = git_source_view_get_property;

  widget_class->realize = git_source_view_realize;
  widget_class->expose_event = git_source_view_expose_event;
  widget_class->size_allocate = git_source_view_size_allocate;
  widget_class->query_tooltip = git_source_view_query_tooltip;
  widget_class->motion_notify_event = git_source_view_motion_notify_event;
  widget_class->button_release_event = git_source_view_button_release_event;

  klass->set_scroll_adjustments = git_source_view_set_scroll_adjustments;

  pspec = g_param_spec_enum ("state",
			     "State",
			     "The state of loading the annotated source. "
			     "Either ready, loading or error.",
			     GIT_TYPE_SOURCE_VIEW_STATE,
			     GIT_SOURCE_VIEW_READY,
			     G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_STATE, pspec);

  client_signals[SET_SCROLL_ADJUSTMENTS]
    = g_signal_new ("set_scroll_adjustments",
		    G_OBJECT_CLASS_TYPE (gobject_class),
		    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		    G_STRUCT_OFFSET (GitSourceViewClass,
				     set_scroll_adjustments),
		    NULL, NULL,
		    _git_marshal_VOID__OBJECT_OBJECT,
		    G_TYPE_NONE, 2,
		    GTK_TYPE_ADJUSTMENT,
		    GTK_TYPE_ADJUSTMENT);
  widget_class->set_scroll_adjustments_signal
    = client_signals[SET_SCROLL_ADJUSTMENTS];

  client_signals[COMMIT_SELECTED]
    = g_signal_new ("commit-selected",
		    G_OBJECT_CLASS_TYPE (gobject_class),
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GitSourceViewClass, commit_selected),
		    NULL, NULL,
		    _git_marshal_VOID__OBJECT,
		    G_TYPE_NONE, 1,
		    GIT_TYPE_COMMIT);

  g_type_class_add_private (klass, sizeof (GitSourceViewPrivate));
}

static void
git_source_view_init (GitSourceView *self)
{
  GitSourceViewPrivate *priv;

  priv = self->priv = GIT_SOURCE_VIEW_GET_PRIVATE (self);

  g_object_set (self, "has-tooltip", TRUE, NULL);

  priv->state = GIT_SOURCE_VIEW_READY;
  priv->state_error = NULL;
}

static void
git_source_view_unref_loading_source (GitSourceView *sview)
{
  GitSourceViewPrivate *priv = sview->priv;

  if (priv->load_source)
    {
      g_signal_handler_disconnect (priv->load_source,
				   priv->loading_completed_handler);
      g_object_unref (priv->load_source);
      priv->load_source = NULL;
    }
}

static void
git_source_view_unref_hadjustment (GitSourceView *sview)
{
  GitSourceViewPrivate *priv = sview->priv;

  if (priv->hadjustment)
    {
      g_signal_handler_disconnect (priv->hadjustment,
				   priv->hadjustment_value_changed_handler);
      g_signal_handler_disconnect (priv->hadjustment,
				   priv->hadjustment_changed_handler);
      g_object_unref (priv->hadjustment);
      priv->hadjustment = NULL;
    }
}

static void
git_source_view_unref_vadjustment (GitSourceView *sview)
{
  GitSourceViewPrivate *priv = sview->priv;

  if (priv->vadjustment)
    {
      g_signal_handler_disconnect (priv->vadjustment,
				   priv->vadjustment_value_changed_handler);
      g_signal_handler_disconnect (priv->vadjustment,
				   priv->vadjustment_changed_handler);
      g_object_unref (priv->vadjustment);
      priv->vadjustment = NULL;
    }
}

static void
git_source_view_dispose (GObject *object)
{
  GitSourceView *self = (GitSourceView *) object;
  GitSourceViewPrivate *priv = self->priv;
  
  if (priv->paint_source)
    {
      g_object_unref (priv->paint_source);
      priv->paint_source = NULL;
    }

  git_source_view_unref_hadjustment (self);
  git_source_view_unref_vadjustment (self);

  git_source_view_unref_loading_source (self);

  if (priv->state_error)
    {
      g_error_free (priv->state_error);
      priv->state_error = NULL;
    }

  if (priv->hand_cursor)
    {
      gdk_cursor_unref (priv->hand_cursor);
      priv->hand_cursor = NULL;
    }

  G_OBJECT_CLASS (git_source_view_parent_class)->dispose (object);
}

static void
git_source_view_set_text_for_line (PangoLayout *layout,
				   const GitAnnotatedSourceLine *line)
{
  int len = strlen (line->text);

  /* Remove any trailing spaces in the text */
  while (len > 0 && isspace (line->text[len - 1]))
    len--;

  pango_layout_set_text (layout, line->text, len);
  pango_layout_set_attributes (layout, NULL);
}

static void
git_source_view_set_text_for_commit (PangoLayout *layout, GitCommit *commit)
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
      if (len > GIT_SOURCE_VIEW_COMMIT_HASH_LENGTH)
	len = GIT_SOURCE_VIEW_COMMIT_HASH_LENGTH;

      pango_layout_set_text (layout, hash, len);
      pango_layout_set_attributes (layout, NULL);
    }
}

static void
git_source_view_update_scroll_adjustments (GitSourceView *sview)
{
  GitSourceViewPrivate *priv = sview->priv;
  GtkWidget *widget = (GtkWidget *) sview;
  gsize n_lines = 0;

  if (priv->paint_source)
    n_lines = git_annotated_source_get_n_lines (priv->paint_source);

  if (priv->hadjustment)
    {
      priv->hadjustment->lower = 0.0;
      priv->hadjustment->upper = priv->max_line_width;
      priv->hadjustment->step_increment = 10.0;
      priv->hadjustment->page_increment = widget->allocation.width;
      priv->hadjustment->page_size = widget->allocation.width
	- priv->max_hash_length - GIT_SOURCE_VIEW_GAP;

      if (priv->hadjustment->value + priv->hadjustment->page_size
	  > priv->hadjustment->upper)
	priv->hadjustment->value
	  = priv->hadjustment->upper - priv->hadjustment->page_size;
      if (priv->hadjustment->value < 0.0)
	priv->hadjustment->value = 0.0;

      gtk_adjustment_changed (priv->hadjustment);
    }
  if (priv->vadjustment)
    {
      priv->vadjustment->lower = 0.0;
      priv->vadjustment->upper = priv->line_height * n_lines;
      priv->vadjustment->step_increment = priv->line_height;
      priv->vadjustment->page_increment = widget->allocation.height;
      priv->vadjustment->page_size = widget->allocation.height;

      if (priv->vadjustment->value + priv->vadjustment->page_size
	  > priv->vadjustment->upper)
	priv->vadjustment->value
	  = priv->vadjustment->upper - priv->vadjustment->page_size;
      if (priv->vadjustment->value < 0.0)
	priv->vadjustment->value = 0.0;

      gtk_adjustment_changed (priv->vadjustment);
    }
}

static void
git_source_view_calculate_line_height (GitSourceView *sview)
{
  GitSourceViewPrivate *priv = sview->priv;

  if (priv->line_height == 0 && GTK_WIDGET_REALIZED (sview)
      && priv->paint_source)
    {
      PangoLayout *layout = gtk_widget_create_pango_layout (GTK_WIDGET (sview),
							    NULL);
      int line_num;
      PangoRectangle logical_rect;
      guint line_height = 1, max_line_width = 1, max_hash_length = 1;

      for (line_num = 0;
	   line_num < git_annotated_source_get_n_lines (priv->paint_source);
	   line_num++)
	{
	  const GitAnnotatedSourceLine *line
	    = git_annotated_source_get_line (priv->paint_source, line_num);

	  git_source_view_set_text_for_line (layout, line);
	  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	  
	  if (logical_rect.height > line_height)
	    line_height = logical_rect.height;
	  if (logical_rect.width > max_line_width)
	    max_line_width = logical_rect.width;

	  git_source_view_set_text_for_commit (layout, line->commit);
	  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	  
	  if (logical_rect.height > line_height)
	    line_height = logical_rect.height;
	  if (logical_rect.width > max_hash_length)
	    max_hash_length = logical_rect.width;
	}

      priv->line_height = line_height;
      priv->max_line_width = max_line_width;
      priv->max_hash_length = max_hash_length + GIT_SOURCE_VIEW_GAP * 2;

      g_object_unref (layout);

      git_source_view_update_scroll_adjustments (sview);
    }
}

static void
git_source_view_realize (GtkWidget *widget)
{
  GdkWindowAttr attribs;

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  memset (&attribs, 0, sizeof (attribs));
  attribs.x = widget->allocation.x;
  attribs.y = widget->allocation.y;
  attribs.width = widget->allocation.width;
  attribs.height = widget->allocation.height;
  attribs.wclass = GDK_INPUT_OUTPUT;
  attribs.window_type = GDK_WINDOW_CHILD;
  attribs.visual = gtk_widget_get_visual (widget);
  attribs.colormap = gtk_widget_get_colormap (widget);
  attribs.event_mask = gtk_widget_get_events (widget)
    | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK
    | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK
    | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attribs,
				   GDK_WA_X | GDK_WA_Y
				   | GDK_WA_VISUAL | GDK_WA_COLORMAP);

  gdk_window_set_user_data (widget->window, widget);
  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_background (widget->window,
			     &widget->style->base[GTK_WIDGET_STATE (widget)]);

  git_source_view_calculate_line_height (GIT_SOURCE_VIEW (widget));
}

static gboolean
git_source_view_expose_event (GtkWidget *widget,
			      GdkEventExpose *event)
{
  GitSourceView *sview = (GitSourceView *) widget;
  GitSourceViewPrivate *priv = sview->priv;
  gsize line_start, line_end, line_num, n_lines;
  gint y;
  PangoLayout *layout;
  cairo_t *cr;

  if (priv->paint_source && priv->line_height)
    {
      layout = gtk_widget_create_pango_layout (widget, NULL);
      cr = gdk_cairo_create (widget->window);

      n_lines = git_annotated_source_get_n_lines (priv->paint_source);
      line_start = (event->area.y + priv->y_offset) / priv->line_height;
      line_end = (event->area.y + priv->y_offset
		  + event->area.height + priv->line_height - 1)
	/ priv->line_height;

      if (line_end > n_lines)
	line_end = n_lines;
      if (line_start > line_end)
	line_start = line_end;

      for (line_num = line_start; line_num < line_end; line_num++)
	{
	  GdkRectangle clip_rect;
	  const GitAnnotatedSourceLine *line
	    = git_annotated_source_get_line (priv->paint_source, line_num);
	  GdkColor color;
	  y = line_num * priv->line_height - priv->y_offset;

	  git_source_view_set_text_for_commit (layout, line->commit);
	  git_commit_get_color (line->commit, &color);

	  cairo_set_source_rgb (cr, color.red / 65535.0, color.green / 65535.0,
				color.blue / 65535.0);
	  cairo_rectangle (cr, 0, y, priv->max_hash_length, priv->line_height);
	  cairo_fill_preserve (cr);

	  /* Invert the color so that the text is guaranteed to be a
	     different (albeit clashing) colour */
	  color.red = ~color.red;
	  color.green = ~color.green;
	  color.blue = ~color.blue;
	  cairo_set_source_rgb (cr, color.red / 65535.0, color.green / 65535.0,
				color.blue / 65535.0);
	  cairo_save (cr);
	  cairo_clip (cr);
	  cairo_move_to (cr, 0, y);
	  pango_cairo_show_layout (cr, layout);
	  cairo_restore (cr);

	  git_source_view_set_text_for_line (layout, line);

	  clip_rect.x = priv->max_hash_length + GIT_SOURCE_VIEW_GAP;
	  clip_rect.width = widget->allocation.width;
	  clip_rect.y = y;
	  clip_rect.height = priv->line_height;

	  gtk_paint_layout (widget->style,
			    widget->window,
			    GTK_WIDGET_STATE (widget),
			    TRUE,
			    &clip_rect,
			    widget,
			    NULL,
			    -priv->x_offset + priv->max_hash_length
			    + GIT_SOURCE_VIEW_GAP,
			    y,
			    layout);
	}

      cairo_destroy (cr);
      g_object_unref (layout);
    }

  return FALSE;
}

static void
git_source_view_on_adj_changed (GtkAdjustment *adj, GitSourceView *sview)
{
  GitSourceViewPrivate *priv = sview->priv;

  if (priv->hadjustment)
    priv->x_offset = priv->hadjustment->value;
  if (priv->vadjustment)
    priv->y_offset = priv->vadjustment->value;

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (sview)))
    gdk_window_invalidate_rect (GTK_WIDGET (sview)->window, NULL, FALSE);
}

static void
git_source_view_on_adj_value_changed (GtkAdjustment *adj, GitSourceView *sview)
{
  GitSourceViewPrivate *priv = sview->priv;
  gint dx = 0, dy = 0;

  if (priv->hadjustment)
    {
      gint new_offset = (gint) priv->hadjustment->value;
      dx = priv->x_offset - new_offset;
      priv->x_offset = new_offset;
    }
  if (priv->vadjustment)
    {
      gint new_offset = (gint) priv->vadjustment->value;
      dy = priv->y_offset - new_offset;
      priv->y_offset = new_offset;
    }

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (sview)))
    {
      /* If dx has changed then we have to redraw the whole window
	 because the commit hashes on the left side shouldn't
	 scroll */
      if (dx)
	gdk_window_invalidate_rect (GTK_WIDGET (sview)->window, NULL, FALSE);
      else if (dy)
	gdk_window_scroll (GTK_WIDGET (sview)->window, dx, dy);
    }
}

static void
git_source_view_set_scroll_adjustments (GtkWidget *widget,
					GtkAdjustment *hadjustment,
					GtkAdjustment *vadjustment)
{
  GitSourceView *sview = (GitSourceView *) widget;
  GitSourceViewPrivate *priv = sview->priv;

  if (hadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadjustment));
  else
    hadjustment
      = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  if (vadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadjustment));
  else
    vadjustment
      = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));  

  g_object_ref_sink (hadjustment);
  g_object_ref_sink (vadjustment);

  git_source_view_unref_hadjustment (sview);
  priv->hadjustment = hadjustment;
  priv->hadjustment_changed_handler
    = g_signal_connect (hadjustment, "changed",
			G_CALLBACK (git_source_view_on_adj_changed),
			sview);
  priv->hadjustment_value_changed_handler
    = g_signal_connect (hadjustment, "value-changed",
			G_CALLBACK (git_source_view_on_adj_value_changed),
			sview);

  git_source_view_unref_vadjustment (sview);
  priv->vadjustment = vadjustment;
  priv->vadjustment_changed_handler
    = g_signal_connect (vadjustment, "changed",
			G_CALLBACK (git_source_view_on_adj_changed),
			sview);
  priv->vadjustment_value_changed_handler
    = g_signal_connect (vadjustment, "value-changed",
			G_CALLBACK (git_source_view_on_adj_value_changed),
			sview);


  git_source_view_update_scroll_adjustments (sview);
}

static void
git_source_view_size_allocate (GtkWidget *widget,
			       GtkAllocation *allocation)
{
  /* Chain up */
  GTK_WIDGET_CLASS (git_source_view_parent_class)
    ->size_allocate (widget, allocation);

  git_source_view_update_scroll_adjustments (GIT_SOURCE_VIEW (widget));
}

static gboolean
git_source_view_query_tooltip (GtkWidget *widget,
			       gint x, gint y,
			       gboolean keyboard_tooltip,
			       GtkTooltip *tooltip)
{
  GitSourceView *sview = (GitSourceView *) widget;
  GitSourceViewPrivate *priv = sview->priv;
  gint line_num, num_lines;
  gboolean ret = TRUE;
  GString *markup;
  const gchar *part;
  gchar *part_markup;
  const GitAnnotatedSourceLine *line;
  GitCommit *commit;

  if (priv->line_height < 1 || priv->paint_source == NULL)
    return FALSE;

  num_lines = git_annotated_source_get_n_lines (priv->paint_source);

  line_num = (y + priv->y_offset) / priv->line_height;

  if (x < 0 || x >= priv->max_hash_length
      || line_num < 0 || line_num >= num_lines)
    return FALSE;

  line = git_annotated_source_get_line (priv->paint_source, line_num);
  commit = line->commit;

  markup = g_string_new ("");

  if ((part = git_commit_get_prop (commit, "author")))
    {
      part_markup = g_markup_printf_escaped ("<b>%s</b>", part);
      g_string_append (markup, part_markup);
      g_free (part_markup);
    }
  if ((part = git_commit_get_prop (commit, "author-mail")))
    {
      if (markup->len > 0)
	g_string_append_c (markup, ' ');
      part_markup = g_markup_escape_text (part, -1);
      g_string_append (markup, part_markup);
      g_free (part_markup);
    }
  if ((part = git_commit_get_prop (commit, "author-time")))
    {
      GTimeVal time_;
      char *tail;

      errno = 0;
      time_.tv_sec = strtol (part, &tail, 10);
      time_.tv_usec = 0;

      if (errno == 0 && *tail == '\0')
	{
	  gchar *display_time;

	  if (markup->len > 0)
	    g_string_append_c (markup, '\n');

	  display_time = git_format_time_for_display (&time_);
	  part_markup = g_markup_escape_text (display_time, -1);
	  g_free (display_time);
	  g_string_append (markup, part_markup);
	  g_free (part_markup);
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

      part_markup = g_markup_printf_escaped ("<i>%s</i>", stripped_part);
      g_free (stripped_part);
      g_string_append (markup, part_markup);
      g_free (part_markup);
    }

  if (markup->len > 0)
    {
      GdkRectangle tip_area;
      
      tip_area.x = 0;
      tip_area.y = line_num * priv->line_height - priv->y_offset;
      tip_area.width = priv->max_hash_length;
      tip_area.height = priv->line_height;
      gtk_tooltip_set_markup (tooltip, markup->str);
      gtk_tooltip_set_tip_area (tooltip, &tip_area);
    }
  else
    ret = FALSE;

  g_string_free (markup, TRUE);

  return ret;
}

static void
git_source_view_get_property (GObject *object, guint property_id,
			      GValue *value, GParamSpec *pspec)
{
  GitSourceView *sview = (GitSourceView *) object;
  GitSourceViewPrivate *priv = sview->priv;

  switch (property_id)
    {
    case PROP_STATE:
      g_value_set_enum (value, priv->state);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

GtkWidget *
git_source_view_new (void)
{
  GtkWidget *widget = g_object_new (GIT_TYPE_SOURCE_VIEW, NULL);

  return widget;
}

static void
git_source_view_set_state (GitSourceView *sview,
			   GitSourceViewState state,
			   const GError *error)
{
  GitSourceViewPrivate *priv = sview->priv;

  priv->state = state;
  if (priv->state_error)
    g_error_free (priv->state_error);
  if (error)
    priv->state_error = g_error_copy (error);

  g_object_notify (G_OBJECT (sview), "state");
}

static void
git_source_view_on_completed (GitAnnotatedSource *source,
			      const GError *error,
			      GitSourceView *sview)
{
  GitSourceViewPrivate *priv = sview->priv;

  if (error)
    git_source_view_set_state (sview, GIT_SOURCE_VIEW_ERROR, error);
  else
    {
      /* Forget the old painting source */
      if (priv->paint_source)
	g_object_unref (priv->paint_source);
      /* Use the loading source to paint with */
      priv->paint_source = g_object_ref (source);

      /* Recalculate the line height */
      priv->line_height = 0;
      git_source_view_calculate_line_height (sview);

      gdk_window_invalidate_rect (GTK_WIDGET (sview)->window, NULL, FALSE);
      git_source_view_update_scroll_adjustments (sview);

      git_source_view_set_state (sview, GIT_SOURCE_VIEW_READY, NULL);
    }

  git_source_view_unref_loading_source (sview);
}

void
git_source_view_set_file (GitSourceView *sview,
			  const gchar *filename,
			  const gchar *revision)
{
  GitSourceViewPrivate *priv;
  GError *error = NULL;

  g_return_if_fail (GIT_IS_SOURCE_VIEW (sview));
  g_return_if_fail (filename != NULL);

  priv = sview->priv;

  /* If we're currently trying to load some source then cancel it */
  git_source_view_unref_loading_source (sview);

  priv->load_source = git_annotated_source_new ();
  priv->loading_completed_handler
    = g_signal_connect (priv->load_source, "completed",
			G_CALLBACK (git_source_view_on_completed), sview);

  if (!git_annotated_source_fetch (priv->load_source,
				   filename, revision,
				   &error))
    {
      git_source_view_set_state (sview, GIT_SOURCE_VIEW_ERROR, error);
      git_source_view_unref_loading_source (sview);
      
      g_error_free (error);
    }
  else
    git_source_view_set_state (sview, GIT_SOURCE_VIEW_LOADING, NULL);
}

GitSourceViewState
git_source_view_get_state (GitSourceView *sview)
{
  g_return_val_if_fail (GIT_IS_SOURCE_VIEW (sview), 0);

  return sview->priv->state;
}

const GError *
git_source_view_get_state_error (GitSourceView *sview)
{
  g_return_val_if_fail (GIT_IS_SOURCE_VIEW (sview), 0);

  return sview->priv->state_error;
}

static gboolean
git_source_view_motion_notify_event (GtkWidget *widget,
				     GdkEventMotion *event)
{
  GitSourceView *sview = (GitSourceView *) widget;
  GitSourceViewPrivate *priv = sview->priv;
  gboolean show_cursor = FALSE;

  /* Show the hand cursor when the pointer is over a commit hash */
  if (priv->paint_source)
    {
      int n_lines = git_annotated_source_get_n_lines (priv->paint_source);

      if (event->y < priv->y_offset + priv->line_height * n_lines
	  && event->x < priv->max_hash_length)
	show_cursor = TRUE;
    }

  if (show_cursor)
    {
      if (!priv->hand_cursor_set)
	{
	  /* Create the hand cursor if we haven't already got
	     one. This will be unref'd in the dispose handler */
	  if (priv->hand_cursor == NULL)
	    priv->hand_cursor
	      = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
					    GDK_HAND2);
	  gdk_window_set_cursor (widget->window, priv->hand_cursor);

	  priv->hand_cursor_set = TRUE;
	}
    }
  else
    {
      if (priv->hand_cursor_set)
	{
	  gdk_window_set_cursor (widget->window, NULL);
	  priv->hand_cursor_set = FALSE;
	}
    }

  return FALSE;
}

static gboolean
git_source_view_button_release_event (GtkWidget *widget,
				      GdkEventButton *event)
{
  GitSourceView *sview = (GitSourceView *) widget;
  GitSourceViewPrivate *priv = sview->priv;

  if (priv->paint_source && priv->line_height > 0)
    {
      gint n_lines = git_annotated_source_get_n_lines (priv->paint_source);
      gint line_num = (event->y + priv->y_offset) / priv->line_height;

      if (line_num >= 0 && line_num < n_lines
	  && event->x < priv->max_hash_length)
	{
	  const GitAnnotatedSourceLine *line
	    = git_annotated_source_get_line (priv->paint_source, line_num);

	  g_signal_emit (sview, client_signals[COMMIT_SELECTED],
			 0, line->commit);
	}
    }

  return FALSE;
}
