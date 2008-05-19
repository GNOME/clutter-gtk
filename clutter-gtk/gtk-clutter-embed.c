/* gtk-clutter-embed.c: Embeddable ClutterStage
 *
 * Copyright (C) 2007 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not see <http://www.fsf.org/licensing>.
 *
 * Authors:
 *   Iain Holmes  <iain@openedhand.com>
 *   Emmanuele Bassi  <ebassi@openedhand.com>
 */

/**
 * SECTION:gtk-clutter-embed
 * @short_description: Widget for embedding a Clutter scene
 *
 * #GtkClutterEmbed is a GTK+ widget embedding a #ClutterStage. Using
 * a #GtkClutterEmbed widget is possible to build, show and interact with
 * a scene built using Clutter inside a GTK+ application.
 *
 * <note>To avoid flickering on show, you should call gtk_widget_show()
 * or gtk_widget_realize() before calling clutter_actor_show() on the
 * embedded #ClutterStage actor. This is needed for Clutter to be able
 * to paint on the #GtkClutterEmbed widget.</note>
 *
 * Since: 0.6
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>

#include <gdk/gdk.h>
#include <gtk/gtkmain.h>

#include <clutter/clutter-main.h>
#include <clutter/clutter-stage.h>
#include <clutter/clutter-container.h>

#if defined(HAVE_CLUTTER_GTK_X11)

#include <clutter/x11/clutter-x11.h>
#include <gdk/gdkx.h>

#elif defined(HAVE_CLUTTER_GTK_WIN32)

#include <clutter/clutter-win32.h>
#include <gdk/gdkwin32.h>

#endif /* HAVE_CLUTTER_GTK_{X11,WIN32} */

#include "gtk-clutter-embed.h"

static void clutter_container_iface_init (ClutterContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkClutterEmbed,
                         gtk_clutter_embed,
                         GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init));

#define GTK_CLUTTER_EMBED_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_CLUTTER_EMBED, GtkClutterEmbedPrivate))

struct _GtkClutterEmbedPrivate
{
  ClutterActor *stage;
};

static void
gtk_clutter_embed_send_configure (GtkClutterEmbed *embed)
{
  GtkWidget *widget;
  GdkEvent *event = gdk_event_new (GDK_CONFIGURE);

  widget = GTK_WIDGET (embed);

  event->configure.window = g_object_ref (widget->window);
  event->configure.send_event = TRUE;
  event->configure.x = widget->allocation.x;
  event->configure.y = widget->allocation.y;
  event->configure.width = widget->allocation.width;
  event->configure.height = widget->allocation.height;
  
  gtk_widget_event (widget, event);
  gdk_event_free (event);
}

static void
gtk_clutter_embed_dispose (GObject *gobject)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (gobject)->priv;

  if (priv->stage)
    {
      clutter_actor_destroy (priv->stage);
      priv->stage = NULL;
    }

  G_OBJECT_CLASS (gtk_clutter_embed_parent_class)->dispose (gobject);
}

static void
gtk_clutter_embed_show (GtkWidget *widget)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (widget)->priv;

  /* Make sure the widget is realised before we show */
  gtk_widget_realize (widget);

  GTK_WIDGET_CLASS (gtk_clutter_embed_parent_class)->show (widget);

  clutter_actor_show (priv->stage);
}

static void
gtk_clutter_embed_hide (GtkWidget *widget)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (widget)->priv;

  GTK_WIDGET_CLASS (gtk_clutter_embed_parent_class)->hide (widget);

  clutter_actor_hide (priv->stage);
}

static void
gtk_clutter_embed_realize (GtkWidget *widget)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (widget)->priv; 
  GdkWindowAttr attributes;
  int attributes_mask;
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);

  /* NOTE: GDK_MOTION_NOTIFY above should be safe as Clutter does its own
   *       throtling. 
  */
  attributes.event_mask = gtk_widget_get_events (widget)
    | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
    | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_MOTION_NOTIFY;


  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                   &attributes,
                                   attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  
  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);

#if defined(HAVE_CLUTTER_GTK_X11)
  clutter_x11_set_stage_foreign (CLUTTER_STAGE (priv->stage), 
                                 GDK_WINDOW_XID (widget->window));
#elif defined(HAVE_CLUTTER_GTK_WIN32)
  clutter_win32_set_stage_foreign (CLUTTER_STAGE (priv->stage), 
				   GDK_WINDOW_HWND (widget->window));
#endif /* HAVE_CLUTTER_GTK_{X11,WIN32} */

  clutter_redraw (CLUTTER_STAGE (priv->stage));

  gtk_clutter_embed_send_configure (GTK_CLUTTER_EMBED (widget));
}

static void
gtk_clutter_embed_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *allocation)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (widget)->priv;

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
                              allocation->x, allocation->y,
                              allocation->width, allocation->height);

      gtk_clutter_embed_send_configure (GTK_CLUTTER_EMBED (widget));
    }

  clutter_actor_set_size (priv->stage,
                          allocation->width,
                          allocation->height);

  if (CLUTTER_ACTOR_IS_VISIBLE (priv->stage))
    clutter_actor_queue_redraw (priv->stage);
}

static gboolean
gtk_clutter_embed_motion_notify_event (GtkWidget      *widget,
                                       GdkEventMotion *event)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (widget)->priv;
  ClutterEvent cevent = { 0, };

  cevent.type = CLUTTER_MOTION;
  cevent.any.stage = CLUTTER_STAGE (priv->stage);
  cevent.motion.x = event->x;
  cevent.motion.y = event->y;
  cevent.motion.time = event->time;

  clutter_do_event (&cevent);

  /* doh - motion events can push ENTER/LEAVE events onto Clutters
   * internal event queue which we do really ever touch (essentially
   * proxying from gtks queue). The below pumps them back out and
   * processes.
   * *could* be side effects with below though doubful as no other
   * events reach the queue (we shut down event collection). Maybe
   * a peek_mask type call could be even safer. 
  */
  while (clutter_events_pending())
    {
      ClutterEvent *ev = clutter_event_get ();
      if (ev)
        {
          clutter_do_event (ev);
          clutter_event_free (ev);
        }
    }

  return FALSE;
}

static gboolean
gtk_clutter_embed_button_event (GtkWidget      *widget,
                                GdkEventButton *event)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (widget)->priv;
  ClutterEvent cevent = { 0, };

  if (event->type == GDK_BUTTON_PRESS ||
      event->type == GDK_2BUTTON_PRESS ||
      event->type == GDK_3BUTTON_PRESS)
    cevent.type = cevent.button.type = CLUTTER_BUTTON_PRESS;
  else if (event->type == GDK_BUTTON_RELEASE)
    cevent.type = cevent.button.type = CLUTTER_BUTTON_RELEASE;
  else
    return FALSE;

  cevent.any.stage = CLUTTER_STAGE (priv->stage);
  cevent.button.x = event->x;
  cevent.button.y = event->y;
  cevent.button.time = event->time;
  cevent.button.click_count =
    (event->type == GDK_BUTTON_PRESS ? 1
                                     : (event->type == GDK_2BUTTON_PRESS ? 2
                                                                         : 3));
  cevent.button.modifier_state = event->state;
  cevent.button.button = event->button;

  clutter_do_event (&cevent);

  return FALSE;
}

static gboolean
gtk_clutter_embed_key_event (GtkWidget   *widget,
                             GdkEventKey *event)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (widget)->priv;
  ClutterEvent cevent = { 0, };

  if (event->type == GDK_KEY_PRESS)
    cevent.type = cevent.key.type = CLUTTER_KEY_PRESS;
  else if (event->type == GDK_KEY_RELEASE)
    cevent.type = cevent.key.type = CLUTTER_KEY_RELEASE;
  else
    return FALSE;

  cevent.any.stage = CLUTTER_STAGE (priv->stage);
  cevent.key.time = event->time;
  cevent.key.modifier_state = event->state;
  cevent.key.keyval = event->keyval;
  cevent.key.hardware_keycode = event->hardware_keycode;

  clutter_do_event (&cevent);

  return FALSE;
}

static gboolean
gtk_clutter_embed_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (widget)->priv;

  if (CLUTTER_ACTOR_IS_VISIBLE (priv->stage))
    clutter_actor_queue_redraw (priv->stage);

  return TRUE;
}

static gboolean
gtk_clutter_embed_map_event (GtkWidget	     *widget,
                             GdkEventAny     *event)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (widget)->priv;

  /* The backend wont get the XEvent as we go strait to do_event().
   * So we have to make sure we set the event here.
  */
  CLUTTER_ACTOR_SET_FLAGS (priv->stage, CLUTTER_ACTOR_MAPPED);

  return TRUE;
}

static void
gtk_clutter_embed_add (ClutterContainer *container,
                       ClutterActor     *actor)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (container)->priv;
  ClutterContainer *stage = CLUTTER_CONTAINER (priv->stage);

  clutter_container_add_actor (stage, actor);
  g_signal_emit_by_name (container, "actor-added", actor);
}

static void
gtk_clutter_embed_remove (ClutterContainer *container,
                          ClutterActor     *actor)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (container)->priv;
  ClutterContainer *stage = CLUTTER_CONTAINER (priv->stage);

  g_object_ref (actor);

  clutter_container_remove_actor (stage, actor);
  g_signal_emit_by_name (container, "actor-removed", actor);

  g_object_unref (actor);
}

static void
gtk_clutter_embed_foreach (ClutterContainer *container,
                           ClutterCallback   callback,
                           gpointer          callback_data)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (container)->priv;
  ClutterContainer *stage = CLUTTER_CONTAINER (priv->stage);

  clutter_container_foreach (stage, callback, callback_data);
}

static void
gtk_clutter_embed_raise (ClutterContainer *container,
                         ClutterActor     *child,
                         ClutterActor     *sibling)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (container)->priv;
  ClutterContainer *stage = CLUTTER_CONTAINER (priv->stage);

  clutter_container_raise_child (stage, child, sibling);
}

static void
gtk_clutter_embed_lower (ClutterContainer *container,
                         ClutterActor     *child,
                         ClutterActor     *sibling)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (container)->priv;
  ClutterContainer *stage = CLUTTER_CONTAINER (priv->stage);

  clutter_container_lower_child (stage, child, sibling);
}

static void
gtk_clutter_embed_sort_depth_order (ClutterContainer *container)
{
  GtkClutterEmbedPrivate *priv = GTK_CLUTTER_EMBED (container)->priv;
  ClutterContainer *stage = CLUTTER_CONTAINER (priv->stage);

  clutter_container_sort_depth_order (stage);
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = gtk_clutter_embed_add;
  iface->remove = gtk_clutter_embed_remove;
  iface->foreach = gtk_clutter_embed_foreach;
  iface->raise = gtk_clutter_embed_raise;
  iface->lower = gtk_clutter_embed_lower;
  iface->sort_depth_order = gtk_clutter_embed_sort_depth_order;
}

static void
gtk_clutter_embed_class_init (GtkClutterEmbedClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GtkClutterEmbedPrivate));

  gobject_class->dispose = gtk_clutter_embed_dispose;

  widget_class->size_allocate = gtk_clutter_embed_size_allocate;
  widget_class->realize = gtk_clutter_embed_realize;
  widget_class->show = gtk_clutter_embed_show;
  widget_class->hide = gtk_clutter_embed_hide;
  widget_class->button_press_event = gtk_clutter_embed_button_event;
  widget_class->button_release_event = gtk_clutter_embed_button_event;
  widget_class->key_press_event = gtk_clutter_embed_key_event;
  widget_class->key_release_event = gtk_clutter_embed_key_event;
  widget_class->motion_notify_event = gtk_clutter_embed_motion_notify_event;
  widget_class->expose_event = gtk_clutter_embed_expose_event;
  widget_class->map_event = gtk_clutter_embed_map_event;
}

static void
gtk_clutter_embed_init (GtkClutterEmbed *embed)
{
  GtkClutterEmbedPrivate *priv;

  embed->priv = priv = GTK_CLUTTER_EMBED_GET_PRIVATE (embed);

  /* disable double-buffering: it's automatically provided
   * by OpenGL
   */
  gtk_widget_set_double_buffered (GTK_WIDGET (embed), FALSE);

  /* we always create new stages rather than use the default */
  priv->stage = clutter_stage_new ();

  /* we must realize the stage to get it ready for embedding */
  clutter_actor_realize (priv->stage);

#ifdef HAVE_CLUTTER_GTK_X11
  {
    const XVisualInfo *xvinfo;
    GdkVisual *visual;
    GdkColormap *colormap;

    /* We need to use the colormap from the Clutter visual */
    xvinfo = clutter_x11_get_stage_visual (CLUTTER_STAGE (priv->stage));
    visual = gdk_x11_screen_lookup_visual (gdk_screen_get_default (),
                                           xvinfo->visualid);
    colormap = gdk_colormap_new (visual, FALSE);
    gtk_widget_set_colormap (GTK_WIDGET (embed), colormap);
  }
#endif
}

/**
 * gtk_clutter_init:
 * @argc: pointer to the arguments count, or %NULL
 * @argv: pointer to the arguments vector, or %NULL
 *
 * This function should be called instead of clutter_init() and
 * gtk_init().
 *
 * Return value: %CLUTTER_INIT_SUCCESS on success, a negative integer
 *   on failure.
 *
 * Since: 0.8
 */
ClutterInitError
gtk_clutter_init (int    *argc,
                  char ***argv)
{
  if (!gtk_init_check (argc, argv))
    return CLUTTER_INIT_ERROR_GTK;

#if defined(HAVE_CLUTTER_GTK_X11)
  clutter_x11_set_display (GDK_DISPLAY());
  clutter_x11_disable_event_retrieval ();
#elif defined(HAVE_CLUTTER_GTK_WIN32)
  clutter_win32_disable_event_retrieval ();
#endif /* HAVE_CLUTTER_GTK_{X11,WIN32} */

  return clutter_init (argc, argv);
}

/**
 * gtk_clutter_embed_new:
 *
 * Creates a new #GtkClutterEmbed widget. This widget can be
 * used to build a scene using Clutter API into a GTK+ application.
 *
 * Return value: the newly created #GtkClutterEmbed
 *
 * Since: 0.6
 */
GtkWidget *
gtk_clutter_embed_new (void)
{
  return g_object_new (GTK_TYPE_CLUTTER_EMBED, NULL);
}

/**
 * gtk_clutter_embed_get_stage:
 * @embed: a #GtkClutterEmbed
 *
 * Retrieves the #ClutterStage from @embed. The returned stage can be
 * used to add actors to the Clutter scene.
 *
 * Return value: the Clutter stage. You should never destroy or unref
 *   the returned actor.
 *
 * Since: 0.6
 */
ClutterActor *
gtk_clutter_embed_get_stage (GtkClutterEmbed *embed)
{
  g_return_val_if_fail (GTK_IS_CLUTTER_EMBED (embed), NULL);

  return embed->priv->stage;
}