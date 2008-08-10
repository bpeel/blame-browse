#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtkwidget.h>
#include <string.h>
#include <ctype.h>

#include "git-source-view.h"
#include "git-annotated-source.h"

static void git_source_view_dispose (GObject *object);
static void git_source_view_realize (GtkWidget *widget);
static gboolean git_source_view_expose_event (GtkWidget *widget,
					      GdkEventExpose *event);

G_DEFINE_TYPE (GitSourceView, git_source_view, GTK_TYPE_WIDGET);

#define GIT_SOURCE_VIEW_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GIT_TYPE_SOURCE_VIEW, \
				GitSourceViewPrivate))

struct _GitSourceViewPrivate
{
  GitAnnotatedSource *paint_source, *load_source;
  guint loading_completed_handler;

  guint line_height;
};

static void
git_source_view_class_init (GitSourceViewClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  gobject_class->dispose = git_source_view_dispose;

  widget_class->realize = git_source_view_realize;
  widget_class->expose_event = git_source_view_expose_event;

  g_type_class_add_private (klass, sizeof (GitSourceViewPrivate));
}

static void
git_source_view_init (GitSourceView *self)
{
  self->priv = GIT_SOURCE_VIEW_GET_PRIVATE (self);
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
git_source_view_dispose (GObject *object)
{
  GitSourceView *self = (GitSourceView *) object;
  GitSourceViewPrivate *priv = self->priv;
  
  if (priv->paint_source)
    {
      g_object_unref (priv->paint_source);
      priv->paint_source = NULL;
    }

  git_source_view_unref_loading_source (self);

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
      guint line_height = 1;

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
	}

      priv->line_height = line_height;

      g_object_unref (layout);
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
    | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK;

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

  if (priv->paint_source && priv->line_height)
    {
      layout = gtk_widget_create_pango_layout (widget, NULL);

      n_lines = git_annotated_source_get_n_lines (priv->paint_source);
      line_start = event->area.y / priv->line_height;
      line_end = (event->area.y + event->area.height + priv->line_height - 1)
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
	  y = line_num * priv->line_height;

	  git_source_view_set_text_for_line (layout, line);

	  clip_rect.x = 0;
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
			    0, y,
			    layout);
	}

      g_object_unref (layout);
    }

  return FALSE;
}

GtkWidget *
git_source_view_new (void)
{
  GtkWidget *widget = g_object_new (GIT_TYPE_SOURCE_VIEW, NULL);

  return widget;
}

static void
git_source_view_show_error (GitSourceView *sview, const GError *error)
{
  /* STUB: should show a dialog box */
  g_warning ("error: %s\n", error->message);
}

static void
git_source_view_on_completed (GitAnnotatedSource *source,
			      const GError *error,
			      GitSourceView *sview)
{
  GitSourceViewPrivate *priv = sview->priv;

  if (error)
    git_source_view_show_error (sview, error);
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
      git_source_view_show_error (sview, error);
      git_source_view_unref_loading_source (sview);
      
      g_error_free (error);
    }
}
