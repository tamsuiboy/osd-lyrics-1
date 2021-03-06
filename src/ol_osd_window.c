/*
 * Copyright (C) 2009  Tiger Soldier
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "ol_osd_window.h"
#include "ol_utils.h"
#include "ol_debug.h"

#define OL_OSD_WINDOW_GET_PRIVATE(obj)   (G_TYPE_INSTANCE_GET_PRIVATE  \
                                          ((obj),                      \
                                           ol_osd_window_get_type (),  \
                                           OlOsdWindowPrivate))

enum {
  PROP_0,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_LOCKED,
};

static const OlColor DEFAULT_INACTIVE_COLORS[OL_LINEAR_COLOR_COUNT]= {
  {0.6, 1.0, 1.0},
  {0.0, 0.0, 1.0},
  {0.6, 1.0, 1.0},
};
static const OlColor DEFAULT_ACTIVE_COLORS[OL_LINEAR_COLOR_COUNT]= {
  {0.4, 0.15, 0.0},
  {1.0, 1.0, 0.0},
  {1.0, 0.5, 0.0},
};
static const int MOUSE_TIMER_INTERVAL = 100;

static const int DEFAULT_LINE_HEIGHT = 45;
static const int DEFAULT_HEIGHT = 100;
static const double LINE_PADDING = 0.1;
static const int BORDER_WIDTH = 5;
static const int DEFAULT_WIDTH = 1024;

typedef struct __OlOsdWindowPrivate OlOsdWindowPrivate;
struct __OlOsdWindowPrivate
{
  gint width;
  float xalign;
  float yalign;
  gboolean locked;
  gboolean pressed;
  gboolean composited;          /* whether the screen is composited */
  gulong composited_signal;
  gint mouse_x;
  gint mouse_y;
  gint old_x;
  gint old_y;
  gboolean visible;
  guint mouse_timer_id;
  gboolean mouse_over;
  GdkWindowEdge toolbar_pos;
  GtkAllocation osd_allocation;
  GtkAllocation child_allocation;
};

struct OsdLrc
{
  gint id;
  gchar *lyric;
};

GType ol_osd_window_get_type (void);
static void ol_osd_window_init (OlOsdWindow *self);
static void ol_osd_window_class_init (OlOsdWindowClass *klass);
/** 
 * @brief Destroys an OSD Window
 * 
 * @param widget The OSD Window to destroy
 */
static void ol_osd_window_destroy (GtkObject *object);
static void ol_osd_window_set_property (GObject *object, guint prop_id,
                                        const GValue *value, GParamSpec *pspec);
static void ol_osd_window_get_property (GObject *object, guint prop_id,
                                        GValue *value, GParamSpec *pspec);
static void ol_osd_window_realize (GtkWidget *widget);
static void ol_osd_window_unrealize (GtkWidget *widget);
static void ol_osd_window_map (GtkWidget *widget);
static void ol_osd_window_map_bg (GtkWidget *widget);
static void ol_osd_window_unmap (GtkWidget *widget);
static void ol_osd_window_unmap_bg (GtkWidget *widget);
static void ol_osd_window_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void ol_osd_window_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void ol_osd_window_show (GtkWidget *widget);
static gboolean ol_osd_window_expose (GtkWidget *widget, GdkEventExpose *event);
static gboolean ol_osd_window_bg_expose (GtkWidget *widget, GdkEventExpose *event);
static gboolean ol_osd_window_enter_notify (GtkWidget *widget, GdkEventCrossing *event);
static gboolean ol_osd_window_leave_notify (GtkWidget *widget, GdkEventCrossing *event);
static gboolean ol_osd_window_button_press (GtkWidget *widget, GdkEventButton *event);
static gboolean ol_osd_window_motion_notify (GtkWidget *widget, GdkEventMotion *event);
static gboolean ol_osd_window_button_release (GtkWidget *widget, GdkEventButton *event);
static void ol_osd_window_compute_osd_allocation (OlOsdWindow *osd,
                                                  GtkAllocation *allocation);
static gboolean ol_osd_window_has_lyrics (OlOsdWindow *osd);
static void ol_osd_window_compute_bg_allocation (OlOsdWindow *osd,
                                                 GtkAllocation *osd_alloc,
                                                 GtkAllocation *allocation,
                                                 GtkAllocation *child_alloc);
static void ol_osd_window_compute_alignment (OlOsdWindow *osd, gint x, gint y,
                                             gdouble *xalign, gdouble *yalign);
static int ol_osd_window_compute_osd_height (OlOsdWindow *osd);
static double ol_osd_window_calc_lyric_xpos (OlOsdWindow *osd,
                                             int line,
                                             double percentage);
static void ol_osd_window_paint_lyrics (OlOsdWindow *osd, cairo_t *cr);
static void ol_osd_window_update_osd (OlOsdWindow *osd);

/** 
 * @brief Paint the layout to be OSD style
 *
 * Paint the lyrics, the odd_lrc will be shown upper-left and the even_lrc will be shown bottom-right
 * @param osd An OlOsdWindow
 * @param cr A cairo_t to be painted
 */
static void ol_osd_window_paint (OlOsdWindow *osd);
static void ol_osd_window_reset_shape_pixmap (OlOsdWindow *osd);
static void ol_osd_window_update_shape (OlOsdWindow *osd, int line);
static void ol_osd_window_update_background (OlOsdWindow *osd);
static void ol_osd_window_clear_cairo (cairo_t *cr);
static void ol_osd_window_set_input_shape_mask (OlOsdWindow *osd);
static void ol_osd_draw_lyric_pixmap (OlOsdWindow *osd, GdkPixmap **pixmap, const char *lyric);
static void ol_osd_window_update_lyric_pixmap (OlOsdWindow *osd, int line);
static void ol_osd_window_update_lyric_rect (OlOsdWindow *osd, int line);
static void ol_osd_window_screen_composited_changed (GdkScreen *screen, gpointer userdata);
static gboolean ol_osd_window_mouse_timer (gpointer data);
static void ol_osd_window_update_allocation (OlOsdWindow *osd);
static void ol_osd_window_check_resize (GtkContainer *container);
static void ol_osd_window_check_mouse_leave (OlOsdWindow *osd);
static void _paint_rect (cairo_t *cr, GdkPixbuf *source,
                         double src_x, double src_y,
                         double src_w, double src_h,
                         double des_x, double des_y,
                         double dex_w, double des_h);
static gboolean _point_in_rect (int x, int y, GdkRectangle *rect);

static GtkWidgetClass *parent_class = NULL;


static void
_paint_rect (cairo_t *cr, GdkPixbuf *source,
             double src_x, double src_y,
             double src_w, double src_h,
             double des_x, double des_y,
             double des_w, double des_h)
{
  ol_assert (cr != NULL);
  ol_assert (source != NULL);
  cairo_save (cr);
  double sw = des_w / src_w;
  double sh = des_h / src_h;
  cairo_translate (cr, des_x, des_y);
  cairo_rectangle (cr, 0, 0,
                   des_w, des_h);
  cairo_scale (cr, sw, sh);
  cairo_clip (cr);
  gdk_cairo_set_source_pixbuf (cr,
                               source,
                               -src_x,
                               -src_y);
  cairo_paint (cr);
  cairo_restore (cr);
}

static gboolean
ol_osd_window_bg_expose (GtkWidget *widget, GdkEventExpose *event)
{
  ol_log_func ();
  ol_assert_ret (OL_IS_OSD_WINDOW (widget), FALSE);
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  int w, h;
  gdk_drawable_get_size (event->window, &w, &h);
  if (osd->bg_pixbuf != NULL)
  {
    int sw, sh;
    cairo_t *cr;
    cr = gdk_cairo_create (event->window);
    if (priv->composited)
    {
      ol_osd_window_clear_cairo (cr);
    }
    sw = gdk_pixbuf_get_width (osd->bg_pixbuf);
    sh = gdk_pixbuf_get_height (osd->bg_pixbuf);
    _paint_rect (cr, osd->bg_pixbuf,
                 0, 0, BORDER_WIDTH, BORDER_WIDTH,
                 0, 0, BORDER_WIDTH, BORDER_WIDTH);
    _paint_rect (cr, osd->bg_pixbuf,
                 0, sh - BORDER_WIDTH, BORDER_WIDTH, BORDER_WIDTH,
                 0, h - BORDER_WIDTH, BORDER_WIDTH, BORDER_WIDTH);
    _paint_rect (cr, osd->bg_pixbuf,
                 sw - BORDER_WIDTH, 0, BORDER_WIDTH, BORDER_WIDTH,
                 w - BORDER_WIDTH, 0, BORDER_WIDTH, BORDER_WIDTH);
    _paint_rect (cr, osd->bg_pixbuf,
                 sw - BORDER_WIDTH, sh - BORDER_WIDTH,
                 BORDER_WIDTH, BORDER_WIDTH,
                 w - BORDER_WIDTH, h - BORDER_WIDTH,
                 BORDER_WIDTH, BORDER_WIDTH);
    _paint_rect (cr, osd->bg_pixbuf,
                 0, BORDER_WIDTH, BORDER_WIDTH, sh - BORDER_WIDTH * 2,
                 0, BORDER_WIDTH, BORDER_WIDTH, h - BORDER_WIDTH * 2);
    _paint_rect (cr, osd->bg_pixbuf,
                 sw - BORDER_WIDTH, BORDER_WIDTH,
                 BORDER_WIDTH, sh - BORDER_WIDTH * 2,
                 w - BORDER_WIDTH, BORDER_WIDTH,
                 BORDER_WIDTH, h - BORDER_WIDTH * 2);
    _paint_rect (cr, osd->bg_pixbuf,
                 BORDER_WIDTH, 0, sw - BORDER_WIDTH * 2, BORDER_WIDTH,
                 BORDER_WIDTH, 0, w - BORDER_WIDTH * 2, BORDER_WIDTH);
    _paint_rect (cr, osd->bg_pixbuf,
                 BORDER_WIDTH, sh - BORDER_WIDTH,
                 sw - BORDER_WIDTH * 2, BORDER_WIDTH,
                 BORDER_WIDTH, h - BORDER_WIDTH,
                 w - BORDER_WIDTH * 2, BORDER_WIDTH);
    _paint_rect (cr, osd->bg_pixbuf,
                 BORDER_WIDTH, BORDER_WIDTH,
                 sw - BORDER_WIDTH * 2, sh - BORDER_WIDTH * 2,
                 BORDER_WIDTH, BORDER_WIDTH,
                 w - BORDER_WIDTH * 2, h - BORDER_WIDTH * 2);
    
    cairo_destroy (cr); 
  }
  else
  {
    gtk_paint_box (widget->style,
                   widget->window,
                   GTK_STATE_NORMAL, GTK_SHADOW_IN,
                   NULL, widget, "buttondefault",
                   0, 0, w, h);

  }
  if (GTK_WIDGET_CLASS (parent_class)->expose_event)
    return GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
}

static void
ol_osd_window_screen_composited_changed (GdkScreen *screen, gpointer userdata)
{
  ol_log_func ();
  ol_assert (OL_IS_OSD_WINDOW (userdata));
  GtkWidget *widget = GTK_WIDGET (userdata);
  OlOsdWindow *osd = OL_OSD_WINDOW (userdata);
  gboolean mapped = GTK_WIDGET_MAPPED (widget);
  if (mapped)
    gtk_widget_unmap (widget);
  if (GTK_WIDGET_REALIZED (widget))
  {
    gtk_widget_unrealize (widget);
    gtk_widget_realize (widget);
  }
  if (mapped)
    gtk_widget_map (widget);
}

GType
ol_osd_window_get_type (void)
{
  static GType ol_osd_type = 0;
  if (!ol_osd_type)
  {
    static const GTypeInfo ol_osd_info =
      {
        sizeof (OlOsdWindowClass),
        NULL,                   /* base_init */
        NULL,                   /* base_finalize */
        (GClassInitFunc) ol_osd_window_class_init,
        NULL,                   /* class_finalize */
        NULL,                   /* class_data */
        sizeof (OlOsdWindow),
        16,                     /* n_preallocs */
        (GInstanceInitFunc) ol_osd_window_init,
      };
    ol_osd_type = g_type_register_static (GTK_TYPE_WINDOW, "OlOsdWindow",
                                          &ol_osd_info, 0);
  }
  return ol_osd_type;
}

static gboolean
ol_osd_window_button_press (GtkWidget *widget, GdkEventButton *event)
{
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  if (GTK_WIDGET_CLASS (parent_class)->button_press_event &&
      GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event))
      return TRUE;
  ol_log_func ();
  if (event->button == 1)
  {
    OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (widget);
    priv->pressed = TRUE;
    priv->old_x = priv->osd_allocation.x - BORDER_WIDTH;
    priv->old_y = priv->osd_allocation.y - BORDER_WIDTH;
    priv->mouse_x = event->x_root;
    priv->mouse_y = event->y_root;
  }
  return FALSE;
}

static gboolean
ol_osd_window_button_release (GtkWidget *widget, GdkEventButton *event)
{
  ol_log_func ();
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  if (GTK_WIDGET_CLASS (parent_class)->button_release_event &&
      GTK_WIDGET_CLASS (parent_class)->button_release_event (widget, event))
      return TRUE;
  if (event->button == 1)
  {
    OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (widget);
    priv->pressed = FALSE;
    /* emit `moved' signal */
    GValue params[1] = {0};
    g_value_init (&params[0], G_OBJECT_TYPE (widget));
    g_value_set_object (&params[0], G_OBJECT (widget));
    g_signal_emitv (params,
                    OL_OSD_WINDOW_GET_CLASS (widget)->signals[OSD_MOVED],
                    0,
                    NULL);
  }
  return FALSE;
}

static gboolean
ol_osd_window_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
  ol_log_func ();
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (widget);
  if (priv->pressed && !priv->locked)
  {
    int x = priv->old_x + (event->x_root - priv->mouse_x);
    int y = priv->old_y + (event->y_root - priv->mouse_y);
    gdouble xalign, yalign;
    OlOsdWindow *osd = OL_OSD_WINDOW (widget);
    ol_osd_window_compute_alignment (osd, x, y, &xalign, &yalign);
    ol_osd_window_set_alignment (osd, xalign, yalign);
  }
  if (GTK_WIDGET_CLASS (parent_class)->motion_notify_event)
    return GTK_WIDGET_CLASS (parent_class)->motion_notify_event (widget, event);
  return FALSE;
}

static gboolean
ol_osd_window_expose (GtkWidget *widget, GdkEventExpose *event)
{
  ol_assert_ret (OL_IS_OSD_WINDOW (widget), FALSE);
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  if (event->window == widget->window)
    ol_osd_window_bg_expose (widget, event);
  else if (event->window == osd->osd_window)
    ol_osd_window_paint (osd);
  return FALSE;
}

static gboolean
ol_osd_window_enter_notify (GtkWidget *widget, GdkEventCrossing *event)
{
  ol_log_func ();
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (widget);
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  if (!priv->locked && !gdk_window_is_visible (widget->window))
  {
    ol_osd_window_map_bg (widget);
    gdk_window_show (osd->osd_window);
  }
  return FALSE;
}

static void
ol_osd_window_check_mouse_leave (OlOsdWindow *osd)
{
  ol_assert (osd != NULL);
  GtkWidget *widget = GTK_WIDGET (osd);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (widget);
  if (!gdk_window_is_visible (widget->window))
    return;
  gint rel_x, rel_y;
  gdk_window_get_pointer (widget->window, &rel_x, &rel_y, NULL);
  GdkRectangle rect;
  rect.x = 0; rect.y = 0;
  rect.width = widget->allocation.width;
  rect.height = widget->allocation.height;
  if (!priv->pressed &&
      !_point_in_rect (rel_x, rel_y, &rect))
  {
    ol_osd_window_unmap_bg (widget);
  }
}

static gboolean
ol_osd_window_leave_notify (GtkWidget *widget, GdkEventCrossing *event)
{
  ol_log_func ();
  ol_debugf ("  (%0.0lf,%0.0lf)\n",
             event->x_root, event->y_root);
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  ol_osd_window_check_mouse_leave (osd);
  return FALSE;
}

static void
ol_osd_window_show (GtkWidget *widget)
{
  ol_log_func ();
  GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);
  gtk_widget_map (widget);
}

static void
ol_osd_window_reset_shape_pixmap (OlOsdWindow *osd)
{
  /* init shape pixmap */
  ol_assert (OL_IS_OSD_WINDOW (osd));
  if (!GTK_WIDGET_REALIZED (GTK_WIDGET (osd)))
    return;
  GtkWidget *widget = GTK_WIDGET (osd);
  gint width, height;
  gdk_drawable_get_size (osd->osd_window, &width, &height);
  if (osd->shape_pixmap != NULL)
  {
    gint w, h;
    gdk_drawable_get_size (osd->shape_pixmap, &w, &h);
    if (w == width && h == height)
    {
      return;
    }
    else
    {
      g_object_unref (osd->shape_pixmap);
      osd->shape_pixmap = NULL;
    }
  }
  osd->shape_pixmap = gdk_pixmap_new (osd->osd_window, width, height, 1);
  /* gdk_drawable_set_colormap (osd->shape_pixmap, */
  /*                            gdk_drawable_get_colormap (widget->window)); */
  cairo_t *cr = gdk_cairo_create (osd->shape_pixmap);
  ol_osd_window_clear_cairo (cr);
  cairo_destroy (cr);
}

static void
ol_osd_window_realize (GtkWidget *widget)
{
  ol_assert (!GTK_WIDGET_REALIZED (widget));
  ol_log_func ();
  OlOsdWindow *osd;
  GdkWindowAttr attr, osd_attr, event_attr;
  GdkWindow *parent_window;
  gint attr_mask, osd_attr_mask, event_attr_mask;

  osd = OL_OSD_WINDOW (widget);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);

  osd->screen = gtk_widget_get_screen (widget);
  if (osd->screen == NULL)
  {
    osd->screen = gdk_screen_get_default ();
  }
  priv->composited_signal =  g_signal_connect (osd->screen, "composited-changed",
                                               G_CALLBACK (ol_osd_window_screen_composited_changed),
                                               osd);
  /* sets rgba colormap */
  GdkColormap* colormap = gdk_screen_get_rgba_colormap (osd->screen);
  if (colormap == NULL)
    colormap = gdk_screen_get_rgb_colormap (osd->screen);
  gtk_widget_set_colormap (widget, colormap);
  
  /* ensure the size allocation */
  ol_osd_window_update_allocation (osd);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  /* GTK_WIDGET_CLASS (parent_class)->realize (widget); */

  priv->composited = gdk_screen_is_composited (osd->screen);

  /* create background window */
  parent_window = gtk_widget_get_root_window (widget);
  
  attr.window_type = GDK_WINDOW_TEMP;
  attr.x = widget->allocation.x;
  attr.y = widget->allocation.y;
  attr.width = widget->allocation.width;
  attr.height = widget->allocation.height;
  attr.visual = gtk_widget_get_visual (widget);
  attr.colormap = gtk_widget_get_colormap (widget);
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.type_hint = GDK_WINDOW_TYPE_HINT_DOCK;
  attr.event_mask = gtk_widget_get_events (widget);
  attr.event_mask |= (GDK_BUTTON_MOTION_MASK |
                      GDK_ENTER_NOTIFY_MASK |
                      GDK_LEAVE_NOTIFY_MASK |
                      GDK_BUTTON_PRESS_MASK |
                      GDK_BUTTON_RELEASE_MASK |
                      GDK_EXPOSURE_MASK);
  attr_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP | GDK_WA_TYPE_HINT;
  
  widget->window = gdk_window_new (parent_window, &attr, attr_mask);
  gdk_window_set_user_data (widget->window, osd);
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  ol_osd_window_update_background (osd);

  /* create event window */
  event_attr = attr;
  event_attr.wclass = GDK_INPUT_ONLY;
  event_attr.override_redirect = TRUE;
  event_attr_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;
  osd->event_window = gdk_window_new (parent_window, &event_attr, attr_mask);
  gdk_window_set_user_data (osd->event_window, osd);
  gdk_window_set_decorations (osd->event_window, 0);

  /* create OSD window */
  osd_attr = attr;
  osd_attr.x = priv->osd_allocation.x;
  osd_attr.y = priv->osd_allocation.y;
  osd_attr.width = priv->osd_allocation.width;
  osd_attr.height = priv->osd_allocation.height;
  osd_attr_mask = attr_mask;
  osd->osd_window = gdk_window_new (parent_window, &osd_attr, osd_attr_mask);
  gdk_window_set_user_data (osd->osd_window, osd);
  
  /* setup input shape mask for osd window */
  ol_osd_window_set_input_shape_mask (osd);
  /* init shape mask pixmap */
  ol_osd_window_reset_shape_pixmap (osd);
  ol_osd_window_update_shape (osd, 0);
}

static void
ol_osd_window_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  OlOsdWindow *osd;
  osd = OL_OSD_WINDOW (widget);
  GtkAllocation alloc, osd_alloc, child_alloc;
  ol_osd_window_compute_osd_allocation (osd, &osd_alloc);
  ol_osd_window_compute_bg_allocation (osd, &osd_alloc, &alloc, &child_alloc);
  requisition->width = alloc.width;
  requisition->height = alloc.height;
}

static void ol_osd_window_unrealize (GtkWidget *widget)
{
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  if (osd->event_window)
  {
    gdk_window_set_user_data (osd->event_window, NULL);
    gdk_window_destroy (osd->event_window);
    osd->event_window = NULL;
  }
  if (osd->osd_window)
  {
    gdk_window_set_user_data (osd->osd_window, NULL);
    gdk_window_destroy (osd->osd_window);
    osd->osd_window = NULL;
  }
  if (priv->composited_signal != 0)
  {
    g_signal_handler_disconnect (osd->screen, priv->composited_signal);
    priv->composited_signal = 0;
  }
  GTK_WIDGET_CLASS(parent_class)->unrealize (widget);
}

static void
ol_osd_window_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  ol_debugf ("%s\n"
             "bg:(%d, %d) - %d x %d\n"
             "osd:(%d, %d) - %d x %d\n"
             "child:(%d, %d) - %d x %d\n",
             __FUNCTION__,
             allocation->x, allocation->y,
             allocation->width, allocation->height,
             priv->osd_allocation.x, priv->osd_allocation.y,
             priv->osd_allocation.width, priv->osd_allocation.height,
             priv->child_allocation.x, priv->child_allocation.y,
             priv->child_allocation.width, priv->child_allocation.height);
  widget->allocation = *allocation;
  GtkBin *bin = GTK_BIN (osd);
  GtkAllocation child_allocation;

  priv->width = allocation->width;
  if (GTK_WIDGET_REALIZED (widget))
  {
    OlOsdWindow *osd = OL_OSD_WINDOW (widget);
    gdk_window_move_resize (osd->osd_window,
                            priv->osd_allocation.x,
                            priv->osd_allocation.y,
                            priv->osd_allocation.width,
                            priv->osd_allocation.height);
    gdk_window_move_resize (osd->event_window,
                            widget->allocation.x,
                            widget->allocation.y,
                            widget->allocation.width,
                            widget->allocation.height);
    gdk_window_move_resize (widget->window,
                            widget->allocation.x,
                            widget->allocation.y,
                            widget->allocation.width,
                            widget->allocation.height);
  }
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
  {
    ol_debugf ("set child allocation\n");
    gtk_widget_size_allocate (bin->child, &priv->child_allocation);
  }
  else
  {
    if (!bin->child)
      ol_debug ("No child");
    else
      ol_debug ("Child is not visible");
  }
}

static gboolean
_point_in_rect (int x, int y, GdkRectangle *rect)
{
  ol_assert_ret (rect != NULL, FALSE);
  return x < rect->x + rect->width &&
    y < rect->y + rect->height &&
    x >= rect->x &&
    y >= rect->y;
}

static gboolean
ol_osd_window_mouse_timer (gpointer data)
{
  /* ol_log_func (); */
  ol_assert_ret (data != NULL, FALSE);
  ol_assert_ret (OL_IS_OSD_WINDOW (data), FALSE);
  OlOsdWindow *osd = OL_OSD_WINDOW (data);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  if (GTK_WIDGET_REALIZED (osd))
  {
    gint rel_x, rel_y;
    gint w, h;
    gboolean mouse_over = FALSE;
    gdk_window_get_pointer (osd->osd_window, &rel_x, &rel_y, NULL);
    /* ol_debugf ("pointer: %d %d\n", rel_x, rel_y); */
    int i;
    /* ol_debugf ("line count: %d\n", osd->line_count); */
    for (i = 0; i < osd->line_count; i++)
    {
      /* ol_debugf ("lyric rect %d: (%d, %d) %d x %d\n", */
      /*            i, */
      /*            osd->lyric_rects[i].x, osd->lyric_rects[i].y, */
      /*            osd->lyric_rects[i].width, osd->lyric_rects[i].height); */
      if (_point_in_rect (rel_x, rel_y, &osd->lyric_rects[i]))
      {
        mouse_over = TRUE;
        /* ol_debug ("mouse over"); */
        break;
      }
    }
    if (priv->mouse_over != mouse_over)
    {
      priv->mouse_over = mouse_over;
      ol_osd_window_update_osd (osd);
    }
    ol_osd_window_check_mouse_leave (osd);
    /* if (priv->mouse_over && !priv->locked && */
    /*     !gdk_window_is_visible (GTK_WIDGET (osd)->window)) */
    /*   ol_osd_window_map_bg (GTK_WIDGET (osd)); */
  }
  return TRUE;
}

static void
ol_osd_window_map (GtkWidget *widget)
{
  ol_log_func ();
  if (GTK_WIDGET_MAPPED (widget))
    return;
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  priv->visible = TRUE;
  if (!priv->locked)
  {
    gdk_window_show (osd->event_window);
    gdk_window_raise (osd->event_window);
  }
  if (ol_osd_window_has_lyrics (osd))
    gdk_window_show (osd->osd_window);
  if (priv->mouse_timer_id == 0)
  {
    ol_debugf ("add timer\n");
    priv->mouse_timer_id = g_timeout_add (MOUSE_TIMER_INTERVAL,
                                          (GSourceFunc) ol_osd_window_mouse_timer,
                                          widget);
  }
}

static void
ol_osd_window_map_bg (GtkWidget *widget)
{
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  gdk_window_show (widget->window);
  GtkBin *bin = GTK_BIN (widget);
  if (bin->child != NULL &&
      GTK_WIDGET_VISIBLE (bin->child) &&
      !GTK_WIDGET_MAPPED (bin->child))
  {
    ol_debug ("Map child");
    gtk_widget_map (bin->child);
  }
  gdk_window_raise (osd->osd_window);
}

static void
ol_osd_window_unmap (GtkWidget *widget)
{
  ol_log_func ();
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  ol_debug ("  hiding windows");
  if (GTK_WIDGET_MAPPED (widget))
  {
    gdk_window_hide (osd->osd_window);
    gdk_window_hide (osd->event_window);
    ol_osd_window_unmap_bg (widget);
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  }
  priv->visible = FALSE;
  ol_debug (" remove timer");
  if (priv->mouse_timer_id != 0)
  {
    g_source_remove (priv->mouse_timer_id);
    priv->mouse_timer_id = 0;
  }
  ol_debug ("  unmap done");
}

static void
ol_osd_window_unmap_bg (GtkWidget *widget)
{
  OlOsdWindow *osd = OL_OSD_WINDOW (widget);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  gdk_window_hide (widget->window);
}

void
ol_osd_window_set_alignment (OlOsdWindow *osd, double xalign, double yalign)
{
  ol_debugf ("%s\n"
             "  xalign %f\n"
             "  yalign %f\n",
             __FUNCTION__,
             xalign,
             yalign
             );
  ol_assert (OL_IS_OSD_WINDOW (osd));
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  priv->xalign = xalign;
  priv->yalign = yalign;
  if (GTK_WIDGET_REALIZED (osd))
  {
    GtkAllocation alloc;
    ol_osd_window_update_allocation (osd);
  }
}

void
ol_osd_window_get_alignment (OlOsdWindow *osd, double *xalign, double *yalign)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  if (xalign == NULL && yalign == NULL)
    return;
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  if (xalign)
    *xalign = priv->xalign;
  if (yalign)
    *yalign = priv->yalign;
}

void
ol_osd_window_resize (OlOsdWindow *osd, gint width, gint height)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  GtkWidget *widget = GTK_WIDGET (osd);
  if (width > 0)
    widget->allocation.width = width;
  if (height > 0)
    widget->allocation.height = height;
  GtkAllocation allo;
  ol_osd_window_update_allocation (osd);
}

void
ol_osd_window_set_width (OlOsdWindow *osd, gint width)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  priv->width = width;
  ol_osd_window_update_allocation (osd);
  ol_osd_window_reset_shape_pixmap (osd);
  ol_osd_window_update_shape (osd, 0);
}

void
ol_osd_window_get_osd_size (OlOsdWindow *osd, gint *width, gint *height)
{
  /* ol_log_func (); */
  ol_assert (OL_IS_OSD_WINDOW (osd));
  if (width == NULL && height == NULL)
    return;
  gint w, h;
  if (GTK_WIDGET_MAPPED (osd))
  {
    gdk_drawable_get_size (osd->osd_window, &w, &h);
  }
  else
  {
    OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
    w = priv->child_allocation.width;
    h = priv->child_allocation.height;
  }
  if (width)
    *width = w;
  if (height)
    *height = h;
}

void
ol_osd_window_set_locked (OlOsdWindow *osd, gboolean locked)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  if (priv->locked == locked)
    return;
  priv->locked = locked;
  if (!GTK_WIDGET_MAPPED (GTK_WIDGET (osd)))
    return;
  if (locked)
  {
    gdk_window_hide (osd->event_window);
    ol_osd_window_unmap_bg (GTK_WIDGET (osd));
  }
  else
  {
    gdk_window_show (osd->event_window);
    if (priv->mouse_over)
    {
      ol_osd_window_map_bg (GTK_WIDGET (osd));
    }
    gdk_window_show (osd->osd_window);
  }
}

static void
ol_osd_window_compute_osd_allocation (OlOsdWindow *osd, GtkAllocation *alloc)
{
  ol_log_func ();
  ol_assert (OL_IS_OSD_WINDOW (osd));
  ol_assert (alloc != NULL);

  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  alloc->width = priv->width - BORDER_WIDTH * 2;
  alloc->height = ol_osd_window_compute_osd_height (osd);
  if (osd->screen != NULL)
  {
    gint screen_width, screen_height;
    screen_width = gdk_screen_get_width (osd->screen);
    screen_height = gdk_screen_get_height (osd->screen);
    OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
    ol_debugf ("  xalign: %f, yalign: %f\n", priv->xalign, priv->yalign);
    alloc->x = priv->xalign * (screen_width - alloc->width - BORDER_WIDTH * 2) +
      BORDER_WIDTH;
    alloc->y = priv->yalign * (screen_height - alloc->height - BORDER_WIDTH * 2) +
      BORDER_WIDTH;
  }
  else
  {
    alloc->x = alloc->y = BORDER_WIDTH;
  }
}

static void
ol_osd_window_compute_bg_allocation (OlOsdWindow *osd, 
                                     GtkAllocation *osd_alloc,
                                     GtkAllocation *alloc,
                                     GtkAllocation *child_alloc)
{
  ol_log_func ();
  ol_assert (OL_IS_OSD_WINDOW (osd));
  ol_assert (osd_alloc != NULL && alloc != NULL &&child_alloc != NULL);

  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  GtkBin *bin;
  bin = GTK_BIN (osd);
  alloc->width = osd_alloc->width + 2 * BORDER_WIDTH;
  alloc->height = osd_alloc->height + 2 * BORDER_WIDTH;
  alloc->x = osd_alloc->x - BORDER_WIDTH;
  alloc->y = osd_alloc->y - BORDER_WIDTH;

  gboolean child_south = TRUE;
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
  {
    GtkRequisition child_requisition;
    gtk_widget_size_request (bin->child, &child_requisition);
    child_alloc->width = alloc->width - BORDER_WIDTH * 2;
    child_alloc->height = child_requisition.height;
    child_alloc->x = BORDER_WIDTH;
    if (osd->screen != NULL)
    {
      gint screen_width, screen_height;
      screen_height = gdk_screen_get_height (osd->screen);
      if (alloc->y + alloc->height + child_requisition.height > screen_height)
        child_south = FALSE;
    }
    if (child_south)
    {
      child_alloc->y = alloc->height - BORDER_WIDTH;
    }
    else
    {
      child_alloc->y = BORDER_WIDTH;
      alloc->y -= child_requisition.height;
    }
    alloc->height += child_requisition.height;
  }
  if (child_south)
    priv->toolbar_pos = GDK_WINDOW_EDGE_SOUTH;
  else
    priv->toolbar_pos = GDK_WINDOW_EDGE_NORTH;
}

static int
ol_osd_window_compute_osd_height (OlOsdWindow *osd)
{
  ol_assert_ret (OL_IS_OSD_WINDOW (osd) && osd->render_context != NULL,
                 DEFAULT_HEIGHT);
  int font_height = ol_osd_render_get_font_height (osd->render_context);
  int height = font_height * (osd->line_count + (osd->line_count - 1) * LINE_PADDING);
  return height;
}

static void
ol_osd_window_check_resize (GtkContainer *container)
{
  ol_osd_window_update_allocation (OL_OSD_WINDOW (container));
}

static void
ol_osd_window_update_allocation (OlOsdWindow *osd)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  GtkAllocation allocation;
  ol_osd_window_compute_osd_allocation (osd, &priv->osd_allocation);
  ol_osd_window_compute_bg_allocation (osd,
                                       &priv->osd_allocation,
                                       &allocation,
                                       &priv->child_allocation);
  gtk_widget_size_allocate (GTK_WIDGET (osd), &allocation);
  gtk_widget_queue_draw (GTK_WIDGET (osd));
  ol_osd_window_update_osd (osd);
}

static void
ol_osd_window_compute_alignment (OlOsdWindow *osd,
                                 gint x, gint y,
                                 gdouble *xalign, gdouble *yalign)
{
  if (!osd || !OL_IS_OSD_WINDOW (osd))
    return;
  GtkWidget *widget = GTK_WIDGET (osd);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  if (xalign)
  {
    gint screen_width = gdk_screen_get_width (osd->screen);
    *xalign = (double) x / (screen_width - priv->osd_allocation.width -
                            BORDER_WIDTH * 2);
    if (*xalign < 0.0)
      *xalign = 0;
    else if (*xalign > 1.0)
      *xalign = 1.0;
  }
  if (yalign)
  {
    gint screen_height = gdk_screen_get_height (osd->screen);
    
    *yalign = (double) y / (screen_height - priv->osd_allocation.height -
                            BORDER_WIDTH * 2);
    if (*yalign < 0.0)
      *yalign = 0;
    else if (*yalign > 1.0)
      *yalign = 1.0;
  }
  ol_debugf ("%s\n"
             "  %lf, %lf\n",
             __FUNCTION__,
             *xalign,
             *yalign);
}

static double
ol_osd_window_calc_lyric_xpos (OlOsdWindow *osd, int line, double percentage)
{
  /* ol_log_func (); */
  GtkWidget *widget = GTK_WIDGET (osd);
  gint w, h;
  int width, height;
  /* gdk_drawable_get_size (widget->window, &w, &h); */
  ol_osd_window_get_osd_size (osd, &w, &h);
  if (osd->active_lyric_pixmap[line] != NULL)
  {
    gdk_drawable_get_size (osd->active_lyric_pixmap[line], &width, &height);
  }
  else
  {
    width = height = 0;
  }
  double xpos;
  if (w >= width)
  {
    xpos = (w - width) * osd->line_alignment[line];
  }
  else
  {
    OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
    if (priv->composited)
    {
      if (percentage * width < w / 2.0)
        xpos = 0;
      else if ((1.0 - percentage) * width < w / 2.0)
        xpos = w - width;
      else
        xpos = w / 2.0 - width * percentage;
    }
    else
    {
      if (percentage * width < w)
      {
        xpos = 0;
      }
      else
      {
        int half_count = (percentage * width - w) / w + 1;
        xpos = -half_count * w;
        if (xpos < w - width)
          xpos = w - width;
      }
    }
  }
  return xpos;
}

static void
ol_osd_window_paint_lyrics (OlOsdWindow *osd, cairo_t *cr)
{
  ol_assert (osd != NULL);
  if (!gdk_window_is_visible (osd->osd_window))
    return;
  GtkWidget *widget = GTK_WIDGET (osd);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  double alpha = 1.0;
  int font_height = ol_osd_render_get_font_height (osd->render_context);
  if (priv->composited && priv->locked && priv->mouse_over &&
      osd->translucent_on_mouse_over)
  {
    alpha = 0.3;
  }
  if (!GTK_WIDGET_REALIZED (widget))
    gtk_widget_realize (widget);
  gint w, h;
  int width, height;
  gdk_drawable_get_size (osd->osd_window, &w, &h);
  int line;
  gdouble ypos = 0, xpos;
  int start, end;
  if (osd->line_count == 1)
  {
    start = osd->current_line;
    end = start + 1;
  }
  else
  {
    start = 0;
    end = OL_OSD_WINDOW_MAX_LINE_COUNT;
  }
  for (line = start; line < end; line++)
  {
    double percentage = osd->percentage[line];
    if (osd->active_lyric_pixmap[line] != NULL && osd->inactive_lyric_pixmap[line])
    {
      gdk_drawable_get_size (osd->active_lyric_pixmap[line], &width, &height);
      xpos = ol_osd_window_calc_lyric_xpos (osd, line, osd->percentage[line]);
      cairo_save (cr);
      cairo_rectangle (cr, xpos, ypos, (double)width * percentage, height);
      cairo_clip (cr);
      gdk_cairo_set_source_pixmap (cr, osd->active_lyric_pixmap[line], xpos, ypos);
      cairo_paint_with_alpha (cr, alpha);
      cairo_restore (cr);
      cairo_save (cr);
      cairo_rectangle (cr, xpos + width * percentage, ypos, (double)width * (1.0 - percentage), height);
      cairo_clip (cr);
      gdk_cairo_set_source_pixmap (cr, osd->inactive_lyric_pixmap[line], xpos, ypos);
      cairo_paint_with_alpha (cr, alpha);
      cairo_restore (cr);
    }
    ypos += font_height * (1 + LINE_PADDING);
  }
}

static void
ol_osd_window_clear_cairo (cairo_t *cr)
{
  ol_assert (cr != NULL);
  cairo_save (cr);
  cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); // set drawing compositing operator
  // SOURCE -> replace destination
  cairo_paint(cr); // paint source
  cairo_restore (cr);
}

void
ol_osd_window_set_percentage (OlOsdWindow *osd, gint line, double percentage)
{
  if (line < 0 || line >= OL_OSD_WINDOW_MAX_LINE_COUNT)
    return;
  ol_assert (OL_IS_OSD_WINDOW (osd));
  if (percentage == osd->percentage[line])
    return;
  double old_percentage = osd->percentage[line];
  osd->percentage[line] = percentage;
  /* update shape if line is too long */
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  if (!priv->composited && percentage != old_percentage)
  {
    int old_x = ol_osd_window_calc_lyric_xpos (osd, line, old_percentage);
    int new_x = ol_osd_window_calc_lyric_xpos (osd, line, percentage);
    if (old_x != new_x)
      ol_osd_window_update_shape (osd, line);
  }
  ol_osd_window_update_osd (osd);
}

void
ol_osd_window_set_current_percentage (OlOsdWindow *osd, double percentage)
{
  ol_osd_window_set_percentage (osd, osd->current_line, percentage);
}

double
ol_osd_window_get_current_percentage (OlOsdWindow *osd)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  return osd->percentage[osd->current_line];
}

void
ol_osd_window_set_current_line (OlOsdWindow *osd, gint line)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  ol_assert (line < ol_osd_window_get_line_count (osd));
  osd->current_line = line;
}

gint ol_osd_window_get_current_line (OlOsdWindow *osd)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  return osd->current_line;
}

static void
ol_osd_draw_lyric_pixmap (OlOsdWindow *osd, GdkPixmap **pixmap, const char *lyric)
{
  if (*pixmap != NULL)
  {
    g_object_unref (*pixmap);
    *pixmap = NULL;
  }
  if (!ol_is_string_empty (lyric) && *pixmap == NULL)
  {
    int w, h;
    if (!GTK_WIDGET_REALIZED (GTK_WIDGET (osd)))
      gtk_widget_realize (GTK_WIDGET (osd));
    ol_osd_render_get_pixel_size (osd->render_context,
                                  lyric,
                                  &w, &h);
    *pixmap = gdk_pixmap_new (osd->osd_window, w, h, -1);
    cairo_t *cr = gdk_cairo_create (*pixmap);
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    ol_osd_render_paint_text (osd->render_context,
                              cr,
                              lyric,
                              0,
                              0);
    cairo_destroy (cr);
  }
}

static void
ol_osd_window_update_lyric_pixmap (OlOsdWindow *osd, int line)
{
  int i;
  if (!GTK_WIDGET_REALIZED (GTK_WIDGET (osd)))
    return;
  /* draws the inactive pixmaps */
  for (i = 0; i < OL_LINEAR_COLOR_COUNT; i++)
  {
    ol_osd_render_set_linear_color (osd->render_context,
                                    i,
                                    osd->inactive_colors[i]);
  }
  ol_osd_draw_lyric_pixmap (osd, &osd->inactive_lyric_pixmap[line], osd->lyrics[line]);
  /* draws the active pixmaps */
  for (i = 0; i < OL_LINEAR_COLOR_COUNT; i++)
  {
    ol_osd_render_set_linear_color (osd->render_context,
                                    i,
                                    osd->active_colors[i]);
  }
  ol_osd_draw_lyric_pixmap (osd, &osd->active_lyric_pixmap[line], osd->lyrics[line]);
  ol_osd_window_update_lyric_rect (osd, line);
}

static void
ol_osd_window_update_lyric_rect (OlOsdWindow *osd, int line)
{
  ol_log_func ();
  int w, h;
  w = h = 0;
  if (osd->active_lyric_pixmap[line] != NULL)
    gdk_drawable_get_size (osd->active_lyric_pixmap[line], &w, &h);
  int font_height = ol_osd_render_get_font_height (osd->render_context);
  osd->lyric_rects[line].x = ol_osd_window_calc_lyric_xpos (osd,
                                                            line,
                                                            osd->percentage[line]);
  osd->lyric_rects[line].y = font_height * line * (1 + LINE_PADDING);
  osd->lyric_rects[line].width = w;
  osd->lyric_rects[line].height = h;
}

static gboolean
ol_osd_window_has_lyrics (OlOsdWindow *osd)
{
  int i;
  gboolean ret = FALSE;
  for (i = 0; i < ol_osd_window_get_line_count (osd); i++)
  {
    if (!ol_is_string_empty (osd->lyrics[i]))
    {
      ret = TRUE;
      break;
    }
  }
  return ret;
}

void
ol_osd_window_set_lyric (OlOsdWindow *osd, gint line, const char *lyric)
{
  ol_log_func ();
  ol_debugf ("  lyric:%s\n", lyric);
  ol_assert (OL_IS_OSD_WINDOW (osd));
  ol_assert (line >= 0 && line < OL_OSD_WINDOW_MAX_LINE_COUNT);
  if (osd->lyrics[line] != NULL)
    g_free (osd->lyrics[line]);
  if (lyric != NULL)
    osd->lyrics[line] = g_strdup (lyric);
  else
    osd->lyrics[line] = g_strdup ("");
  gboolean empty = !ol_osd_window_has_lyrics (osd);
  if (empty && gdk_window_is_visible (osd->osd_window))
  {
    ol_debug ("Lyrics are empty strings");
    gdk_window_hide (osd->osd_window);
  } else if (!empty && !gdk_window_is_visible (osd->osd_window)
             && GTK_WIDGET_VISIBLE (GTK_WIDGET (osd)))
  {
    ol_debug ("Lyrics are not empty strings");
    gdk_window_show (osd->osd_window);
  }
  ol_osd_window_update_lyric_pixmap (osd, line);
  ol_osd_window_update_shape (osd, line);
  ol_osd_window_update_osd (osd);
}

void
ol_osd_window_set_line_alignment (OlOsdWindow *osd, gint line, double alignment)
{
  ol_debugf ("%s:%d-%lf\n", __FUNCTION__, line, alignment);
  ol_assert (OL_IS_OSD_WINDOW (osd));
  if (line < 0 || line >= OL_OSD_WINDOW_MAX_LINE_COUNT)
    return;
  if (alignment < 0.0)
    alignment = 0.0;
  else if (alignment > 1.0)
    alignment = 1.0;
  osd->line_alignment[line] = alignment;
  ol_osd_window_update_lyric_rect (osd, line);
  ol_osd_window_update_shape (osd, line);
  ol_osd_window_update_osd (osd);
}

static void
ol_osd_window_paint (OlOsdWindow *osd)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  GtkWidget *widget = GTK_WIDGET (osd);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  ol_assert (GTK_WIDGET_REALIZED (widget));
  if (!GTK_WIDGET_VISIBLE (widget))
    return;
  cairo_t *cr;
  cr = gdk_cairo_create (osd->osd_window);
  if (priv->composited)
  {
    ol_osd_window_clear_cairo (cr);
  }
  ol_osd_window_paint_lyrics (osd, cr);
  cairo_destroy (cr); 
}

static void
ol_osd_window_update_shape (OlOsdWindow *osd, int line)
{
  /* ol_log_func (); */
  ol_assert (OL_IS_OSD_WINDOW (osd));
  GtkWidget *widget = GTK_WIDGET (osd);
  if (!GTK_WIDGET_REALIZED (widget))
    return;
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  if (priv->composited)
  {
    gdk_window_shape_combine_mask (osd->osd_window, NULL, 0, 0);
    return;
  }
  GdkPixmap *shape_mask = osd->shape_pixmap;
  GdkGC *fg_gc = gdk_gc_new (shape_mask);
  GdkColor color;
  color.pixel = 0;
  gdk_gc_set_foreground (fg_gc, &color);
  gdk_gc_set_background (fg_gc, &color);
  cairo_t *cr = gdk_cairo_create (shape_mask);
  ol_osd_window_clear_cairo (cr);
  ol_osd_window_paint_lyrics (osd, cr);
  cairo_destroy (cr);
  gdk_window_shape_combine_mask (osd->osd_window, shape_mask, 0, 0);
  g_object_unref (fg_gc);
}

static void
ol_osd_window_set_input_shape_mask (OlOsdWindow *osd)
{
  gint w, h;
  ol_assert (OL_IS_OSD_WINDOW (osd));
  GtkWidget *widget = GTK_WIDGET (osd);
  ol_assert (GTK_WIDGET_REALIZED (widget));
  GdkRegion *region = gdk_region_new ();
  gdk_window_input_shape_combine_region (osd->osd_window, region, 0, 0);
  gdk_region_destroy (region);
}

static void
ol_osd_window_class_init (OlOsdWindowClass *klass)
{
  ol_log_func ();
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (klass);
  object_class = (GtkObjectClass*)klass;
  widget_class = (GtkWidgetClass*)klass;
  container_class = (GtkContainerClass*)klass;
  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = ol_osd_window_set_property;
  gobject_class->get_property = ol_osd_window_get_property;

  object_class->destroy = ol_osd_window_destroy;
  
  widget_class->realize = ol_osd_window_realize;
  widget_class->unrealize = ol_osd_window_unrealize;
  widget_class->size_allocate = ol_osd_window_size_allocate;
  widget_class->size_request = ol_osd_window_size_request;
  widget_class->map = ol_osd_window_map;
  widget_class->unmap = ol_osd_window_unmap;
  widget_class->show = ol_osd_window_show;
  widget_class->expose_event = ol_osd_window_expose;
  widget_class->button_press_event = ol_osd_window_button_press;
  widget_class->button_release_event = ol_osd_window_button_release;
  widget_class->motion_notify_event = ol_osd_window_motion_notify;
  widget_class->enter_notify_event = ol_osd_window_enter_notify;
  widget_class->leave_notify_event = ol_osd_window_leave_notify;

  container_class->check_resize = ol_osd_window_check_resize;
  
  /* set up properties */
  g_object_class_install_property (gobject_class,
                                   PROP_XALIGN,
                                   g_param_spec_float ("xalign",
                                                       ("Horizontal alignment for child"),
                                                       ("Horizontal position of window in desktop. "
                                                        "0.0 is left aligned, 1.0 is right aligned"),
                                                       0.0,
                                                       1.0,
                                                       0.5,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_YALIGN,
                                   g_param_spec_float ("yalign",
                                                       ("Horizontal alignment for child"),
                                                       ("Horizontal position of window in desktop. "
                                                        "0.0 is left aligned, 1.0 is right aligned"),
                                                       0.0,
                                                       1.0,
                                                       0.5,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_LOCKED,
                                   g_param_spec_boolean ("locked",
                                                         ("Whether the it is possible to move the OSD window"),
                                                         ("If TRUE, there will be a window displayed under the"
                                                          "OSD indicating that it can be moved, and so it is"),
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));
  /* install signals */
  klass->signals[OSD_MOVED] =
    g_signal_newv ("moved",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  NULL,         /* closure */
                  NULL,         /* accumulator */
                  NULL,         /* accumulator data */
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0,
                  NULL);
  g_type_class_add_private (gobject_class, sizeof (OlOsdWindowPrivate));
}

static void
ol_osd_window_set_property (GObject *object, guint prop_id,
                            const GValue *value, GParamSpec *pspec)
{
  OlOsdWindow *osd = OL_OSD_WINDOW (object);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (osd);
  switch (prop_id)
  {
  case PROP_XALIGN:
    ol_osd_window_set_alignment (osd, g_value_get_float (value), priv->yalign);
    break;
  case PROP_YALIGN:
    ol_osd_window_set_alignment (osd, g_value_get_float (value), priv->xalign);
    break;
  }
}

static void
ol_osd_window_get_property (GObject *object, guint prop_id,
                            GValue *value, GParamSpec *pspec)
{
  OlOsdWindow *osd = OL_OSD_WINDOW (object);
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (priv);
  switch (prop_id)
  {
  case PROP_XALIGN:
    g_value_set_float (value, priv->xalign);
    break;
  case PROP_YALIGN:
    g_value_set_float (value, priv->yalign);
    break;
  case PROP_LOCKED:
    g_value_set_boolean (value, priv->locked);
  }
}
  
static void
ol_osd_window_init (OlOsdWindow *self)
{
  ol_log_func ();
  /* /\* GTK_BIN automately add GTK_NO_WINDOW, we need to unset it *\/ */
  /* GTK_WIDGET_UNSET_FLAGS (self, GTK_NO_WINDOW);  */
  /* GTK_WIDGET_SET_FLAGS (self, /\* GTK_CAN_FOCUS |  *\/GTK_RECEIVES_DEFAULT); */
  /* GTK_WIDGET_SET_FLAGS (self, GTK_TOPLEVEL); */
  /* GTK_PRIVATE_SET_FLAG (self, GTK_ANCHORED); */
  /* gtk_container_set_resize_mode (GTK_CONTAINER (self), GTK_RESIZE_QUEUE); */
  GtkWindow *window = GTK_WINDOW (self);
  gtk_window_set_decorated (window, FALSE);
  window->type = GTK_WINDOW_POPUP;
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DOCK);
  gtk_window_set_has_frame (window, FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (self), TRUE);
  self->screen = NULL;
  /* self->event_window = NULL; */
  self->current_line = 0;
  self->bg_pixbuf = NULL;
  self->line_count = 1;
  int i;
  for (i = 0; i < OL_OSD_WINDOW_MAX_LINE_COUNT; i++)
  {
    self->lyrics[i] = NULL;
    self->line_alignment[i] = 0.5;
    self->percentage[i] = 0.0;
    self->active_lyric_pixmap[i] = NULL;
    self->inactive_lyric_pixmap[i] = NULL;
    self->lyric_rects[i].x = 0;
    self->lyric_rects[i].y = 0;
    self->lyric_rects[i].width = 0;
    self->lyric_rects[i].height = 0;
  }
  for (i = 0; i < OL_LINEAR_COLOR_COUNT; i++)
  {
    self->active_colors[i] = DEFAULT_ACTIVE_COLORS[i];
    self->inactive_colors[i] = DEFAULT_INACTIVE_COLORS[i];
  }
  self->render_context = ol_osd_render_context_new ();
  self->shape_pixmap = NULL;
  self->translucent_on_mouse_over = FALSE;
  /* initilaize private data */
  OlOsdWindowPrivate *priv = OL_OSD_WINDOW_GET_PRIVATE (self);
  priv->width = DEFAULT_WIDTH;
  priv->xalign = priv->yalign = 0.5;
  priv->pressed = FALSE;
  priv->locked = TRUE;
  priv->composited = FALSE;
  priv->visible = FALSE;
  priv->mouse_timer_id = 0;
  priv->mouse_over = FALSE;
  priv->toolbar_pos = GDK_WINDOW_EDGE_SOUTH;
  priv->composited_signal = 0;
}

GtkWidget*
ol_osd_window_new ()
{
  ol_log_func ();
  OlOsdWindow *osd;
  osd = g_object_new (ol_osd_window_get_type (), NULL);
  return GTK_WIDGET (osd);
}

static void
ol_osd_window_destroy (GtkObject *object)
{
  ol_log_func ();
  GtkWidget *widget = GTK_WIDGET (object);
  OlOsdWindow *osd = OL_OSD_WINDOW (object);
  int i;
  for (i = 0; i < OL_OSD_WINDOW_MAX_LINE_COUNT; i++)
  {
    if (osd->lyrics[i] != NULL)
    {
      g_free (osd->lyrics[i]);
      osd->lyrics[i] = NULL;
    }
    if (osd->active_lyric_pixmap[i] != NULL)
    {
      g_object_unref (osd->active_lyric_pixmap[i]);
      osd->active_lyric_pixmap[i] = NULL;
    }
    if (osd->inactive_lyric_pixmap[i] != NULL)
    {
      g_object_unref (osd->inactive_lyric_pixmap[i]);
      osd->inactive_lyric_pixmap[i] = NULL;
    }
  }
  if (osd->shape_pixmap != NULL)
  {
    g_object_unref (osd->shape_pixmap);
    osd->shape_pixmap = NULL;
  }
  if (osd->render_context != NULL)
  {
    ol_osd_render_context_destroy (osd->render_context);
    osd->render_context = NULL;
  }
  if (osd->bg_pixbuf != NULL)
    g_object_unref (osd->bg_pixbuf);
  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

void
ol_osd_window_set_font_family (OlOsdWindow *osd,
                               const char *font_family)
{
  if (osd == NULL || osd->render_context == NULL || font_family == NULL)
    return;
  ol_osd_render_set_font_family (osd->render_context,
                                 font_family);
  int i;
  for (i = 0; i < osd->line_count; i++)
    ol_osd_window_update_lyric_pixmap (osd, i);
  ol_osd_window_update_allocation (osd);
  ol_osd_window_update_shape (osd, 0);
  ol_osd_window_update_osd (osd);
}

char*
ol_osd_window_get_font_family (OlOsdWindow *osd)
{
  if (osd == NULL || osd->render_context == NULL)
    return NULL;
  return ol_osd_render_get_font_family (osd->render_context);
}

void
ol_osd_window_set_font_size (OlOsdWindow *osd,
                             const double font_size)
{
  if (osd == NULL || osd->render_context == NULL)
    return;
  ol_osd_render_set_font_size (osd->render_context, font_size);
  int i;
  for (i = 0; i < osd->line_count; i++)
    ol_osd_window_update_lyric_pixmap (osd, i);
  ol_osd_window_update_allocation (osd);
  ol_osd_window_update_shape (osd, 0);
  ol_osd_window_update_osd (osd);
}

double
ol_osd_window_get_font_size (OlOsdWindow *osd)
{
  ol_assert_ret (osd != NULL, 0.0);
  ol_assert_ret (osd->render_context != NULL, 0.0);
  return ol_osd_render_get_font_size (osd->render_context);
}

void
ol_osd_window_set_outline_width (OlOsdWindow *osd,
                                 const int width)
{
  ol_assert (osd != NULL);
  ol_assert (osd->render_context != NULL);
  ol_osd_render_set_outline_width (osd->render_context, width);
  int i;
  for (i = 0; i < osd->line_count; i++)
    ol_osd_window_update_lyric_pixmap (osd, i);
  ol_osd_window_update_allocation (osd);
  ol_osd_window_update_shape (osd, 0);
}

int
ol_osd_window_get_outline_width (OlOsdWindow *osd)
{
  ol_assert_ret (osd != NULL, 0);
  ol_assert_ret (osd->render_context != NULL, 0);
  return ol_osd_render_get_outline_width (osd->render_context);

}

void
ol_osd_window_set_active_colors (OlOsdWindow *osd,
                                 OlColor top_color,
                                 OlColor middle_color,
                                 OlColor bottom_color)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  osd->active_colors[0] = top_color;
  osd->active_colors[1] = middle_color;
  osd->active_colors[2] = bottom_color;
  int i;
  for (i = 0; i < osd->line_count; i++)
    ol_osd_window_update_lyric_pixmap (osd, i);
  ol_osd_window_update_osd (osd);
}

void
ol_osd_window_set_inactive_colors (OlOsdWindow *osd,
                                   OlColor top_color,
                                   OlColor middle_color,
                                   OlColor bottom_color)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  osd->inactive_colors[0] = top_color;
  osd->inactive_colors[1] = middle_color;
  osd->inactive_colors[2] = bottom_color;
  int i;
  for (i = 0; i < osd->line_count; i++)
    ol_osd_window_update_lyric_pixmap (osd, i);
  ol_osd_window_update_osd (osd);
}

void
ol_osd_window_set_line_count (OlOsdWindow *osd,
                              guint line_count)
{
  ol_assert (OL_IS_OSD_WINDOW (osd)); 
  ol_assert (line_count >= 1 && line_count <= 2);
  osd->line_count = line_count;
  ol_osd_window_update_allocation (osd);
}

guint
ol_osd_window_get_line_count (OlOsdWindow *osd)
{
  ol_assert_ret (OL_IS_OSD_WINDOW (osd), 1);
  return osd->line_count;
}

void
ol_osd_window_set_translucent_on_mouse_over (OlOsdWindow *osd,
                                             gboolean value)
{
  ol_assert (OL_IS_OSD_WINDOW (osd));
  osd->translucent_on_mouse_over = value;
}

gboolean
ol_osd_window_get_translucent_on_mouse_over (OlOsdWindow *osd)
{
  ol_assert_ret (OL_IS_OSD_WINDOW (osd), FALSE);
  return osd->translucent_on_mouse_over;
}

void
ol_osd_window_set_bg (OlOsdWindow *osd, GdkPixbuf *bg)
{
  ol_log_func ();
  ol_debugf ("bg: %p\n", bg);
  ol_assert (OL_IS_OSD_WINDOW (osd));
  if (bg != NULL)
    g_object_ref (bg);
  if (osd->bg_pixbuf != NULL)
    g_object_unref (osd->bg_pixbuf);
  osd->bg_pixbuf = bg;
  ol_osd_window_update_background (osd);
}

static void
ol_osd_window_update_background (OlOsdWindow *osd)
{
  ol_log_func ();
  ol_assert (OL_IS_OSD_WINDOW (osd));
  GtkWidget *widget = GTK_WIDGET (osd);
  /* if (widget->window != NULL) */
  /* { */
  /*   if (osd->bg_pixbuf == NULL) */
  /*     gdk_window_set_opacity (widget->window, 0.5); */
  /*   else */
  /*     gdk_window_set_opacity (widget->window, 1.0); */
  /* } */
}

static void
ol_osd_window_update_osd (OlOsdWindow *osd)
{
  ol_assert (osd != NULL);
  ol_assert (OL_IS_OSD_WINDOW (osd));
  if (osd->osd_window != NULL)
    gdk_window_invalidate_rect (osd->osd_window, NULL, FALSE);
}
