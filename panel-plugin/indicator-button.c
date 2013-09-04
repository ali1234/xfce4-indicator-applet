/*  Copyright (c) 2012-2013 Andrzej <ndrwrdck@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



/*
 *  This file implements an indicator button class corresponding to
 *  a single indicator object entry.
 *
 */



#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libindicator/indicator-object.h>

#include "indicator-button.h"
#include "indicator-button-box.h"


#include <libindicator/indicator-object.h>

#define ICON_SIZE 22
#define SPACING 2

//#ifndef INDICATOR_OBJECT_SIGNAL_ENTRY_SCROLLED
//#define INDICATOR_OBJECT_SIGNAL_ENTRY_SCROLLED "scroll-entry"
//#endif

static void                 xfce_indicator_button_finalize        (GObject                *object);
static gboolean             xfce_indicator_button_button_press    (GtkWidget              *widget,
                                                                   GdkEventButton         *event);
static gboolean             xfce_indicator_button_scroll          (GtkWidget              *widget,
                                                                   GdkEventScroll         *event);
static void                 xfce_indicator_button_menu_deactivate (XfceIndicatorButton    *button,
                                                                   GtkMenu                *menu);
static void            xfce_indicator_button_get_preferred_width  (GtkWidget              *widget,
                                                                   gint                   *minimum_width,
                                                                   gint                   *natural_width);
static void            xfce_indicator_button_get_preferred_height (GtkWidget              *widget,
                                                                   gint                   *minimum_height,
                                                                   gint                   *natural_height);


struct _XfceIndicatorButton
{
  GtkToggleButton       __parent__;

  IndicatorObject      *io;
  const gchar          *io_name;
  IndicatorObjectEntry *entry;
  GtkMenu              *menu;
  XfcePanelPlugin      *plugin;
  IndicatorConfig      *config;

  GtkWidget            *align_box;
  GtkWidget            *box;
};

struct _XfceIndicatorButtonClass
{
  GtkToggleButtonClass __parent__;
};




G_DEFINE_TYPE (XfceIndicatorButton, xfce_indicator_button, GTK_TYPE_TOGGLE_BUTTON)

static void
xfce_indicator_button_class_init (XfceIndicatorButtonClass *klass)
{
  GObjectClass      *gobject_class;
  GtkWidgetClass    *widget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_indicator_button_finalize;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->button_press_event = xfce_indicator_button_button_press;
  widget_class->scroll_event = xfce_indicator_button_scroll;
  widget_class->get_preferred_width = xfce_indicator_button_get_preferred_width;
  widget_class->get_preferred_height = xfce_indicator_button_get_preferred_height;
}



static void
xfce_indicator_button_init (XfceIndicatorButton *button)
{
  //GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT | GTK_CAN_FOCUS);
  gtk_widget_set_can_focus(GTK_WIDGET(button), FALSE);
  gtk_widget_set_can_default (GTK_WIDGET (button), FALSE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_button_set_use_underline (GTK_BUTTON (button),TRUE);
  gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
  gtk_widget_set_name (GTK_WIDGET (button), "indicator-button");

  button->io = NULL;
  button->entry = NULL;
  button->plugin = NULL;
  button->config = NULL;
  button->menu = NULL;

  button->align_box = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
  gtk_widget_set_valign (GTK_WIDGET(button->align_box), GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (button), button->align_box);
  gtk_widget_show (button->align_box);
}



static void
xfce_indicator_button_finalize (GObject *object)
{
  XfceIndicatorButton *button = XFCE_INDICATOR_BUTTON (object);

  xfce_indicator_button_disconnect_signals (button);

  if (button->menu != NULL)
    g_object_unref (G_OBJECT (button->menu));
  /* IndicatorObjectEntry is not GObject */
  /* if (button->entry != NULL) */
  /*   g_object_unref (G_OBJECT (button->entry)); */
  if (button->io != NULL)
    g_object_unref (G_OBJECT (button->io));

  G_OBJECT_CLASS (xfce_indicator_button_parent_class)->finalize (object);
}



void
xfce_indicator_button_set_label (XfceIndicatorButton *button,
                                 GtkLabel            *label)
{
  g_return_if_fail (XFCE_IS_INDICATOR_BUTTON (button));
  g_return_if_fail (GTK_IS_LABEL (label));

  indicator_button_box_set_label (XFCE_INDICATOR_BUTTON_BOX (button->box), label);
}




void
xfce_indicator_button_set_image (XfceIndicatorButton *button,
                                 GtkImage            *image)
{
  g_return_if_fail (XFCE_IS_INDICATOR_BUTTON (button));
  g_return_if_fail (GTK_IS_IMAGE (image));

  indicator_button_box_set_image (XFCE_INDICATOR_BUTTON_BOX (button->box), image);
}



void
xfce_indicator_button_set_menu (XfceIndicatorButton *button,
                                GtkMenu             *menu)
{
  g_return_if_fail (XFCE_IS_INDICATOR_BUTTON (button));
  g_return_if_fail (GTK_IS_MENU (menu));

  if (button->menu != menu)
    {
      if (button->menu != NULL)
        g_object_unref (G_OBJECT (button->menu));
      button->menu = menu;
      g_object_ref (G_OBJECT (button->menu));
      g_signal_connect_swapped (G_OBJECT (button->menu), "deactivate",
                                G_CALLBACK (xfce_indicator_button_menu_deactivate), button);
      gtk_menu_attach_to_widget(menu, GTK_WIDGET (button), NULL);
    }
}



IndicatorObjectEntry *
xfce_indicator_button_get_entry (XfceIndicatorButton *button)
{
  g_return_val_if_fail (XFCE_IS_INDICATOR_BUTTON (button), NULL);

  return button->entry;
}



IndicatorObject *
xfce_indicator_button_get_io (XfceIndicatorButton *button)
{
  g_return_val_if_fail (XFCE_IS_INDICATOR_BUTTON (button), NULL);

  return button->io;
}



const gchar *
xfce_indicator_button_get_io_name (XfceIndicatorButton *button)
{
  g_return_val_if_fail (XFCE_IS_INDICATOR_BUTTON (button), NULL);

  return button->io_name;
}



guint
xfce_indicator_button_get_pos (XfceIndicatorButton *button)
{
  g_return_val_if_fail (XFCE_IS_INDICATOR_BUTTON (button), 0);

  return indicator_object_get_location (button->io, button->entry);
}







GtkMenu *
xfce_indicator_button_get_menu (XfceIndicatorButton *button)
{
  g_return_val_if_fail (XFCE_IS_INDICATOR_BUTTON (button), NULL);

  return button->menu;
}





gboolean
xfce_indicator_button_is_small (XfceIndicatorButton *button)
{
  g_return_val_if_fail (XFCE_IS_INDICATOR_BUTTON (button), FALSE);

  return indicator_button_box_is_small (XFCE_INDICATOR_BUTTON_BOX (button->box));
}




gint
xfce_indicator_button_get_button_border (XfceIndicatorButton  *button)
{
  GtkStyleContext     *ctx;
  GtkBorder            padding, border;

  g_return_val_if_fail (XFCE_IS_INDICATOR_BUTTON (button), 0);

  ctx = gtk_widget_get_style_context (GTK_WIDGET (button));
  gtk_style_context_get_padding (ctx, gtk_widget_get_state_flags (GTK_WIDGET (button)), &padding);
  gtk_style_context_get_border (ctx, gtk_widget_get_state_flags (GTK_WIDGET (button)), &border);

  return MAX (padding.left+padding.right+border.left+border.right,
              padding.top+padding.bottom+border.top+border.bottom);
}


GtkWidget *
xfce_indicator_button_new (IndicatorObject      *io,
                           const gchar          *io_name,
                           IndicatorObjectEntry *entry,
                           XfcePanelPlugin      *plugin,
                           IndicatorConfig      *config)
{
  XfceIndicatorButton *button = g_object_new (XFCE_TYPE_INDICATOR_BUTTON, NULL);
  g_return_val_if_fail (XFCE_IS_INDICATOR_CONFIG (config), NULL);
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), NULL);

  button->io = io;
  button->io_name = io_name;
  button->entry = entry;
  button->plugin = plugin;
  button->config = config;

  button->box = indicator_button_box_new (button->config);
  //gtk_container_add (GTK_CONTAINER (button), button->box);
  gtk_container_add (GTK_CONTAINER (button->align_box), button->box);
  gtk_widget_show (button->box);

  if (button->io != NULL)
    g_object_ref (G_OBJECT (button->io));
  /* IndicatorObjectEntry is not GObject */
  /* g_object_ref (G_OBJECT (button->entry)); */

  return GTK_WIDGET (button);
}



void
xfce_indicator_button_disconnect_signals (XfceIndicatorButton *button)
{
  g_return_if_fail (XFCE_IS_INDICATOR_BUTTON (button));

  if (button->menu != 0)
    {
      gtk_menu_popdown (button->menu);
    }
}


static gboolean
xfce_indicator_button_button_press (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  XfceIndicatorButton *button = XFCE_INDICATOR_BUTTON (widget);

  if(event->button == 1 && button->menu != NULL) /* left click only */
    {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),TRUE);
      gtk_menu_reposition (GTK_MENU (button->menu));
      gtk_menu_popup (button->menu, NULL, NULL,
                      xfce_panel_plugin_position_menu, button->plugin,
                      event->button, event->time);
      return TRUE;
    }

  return FALSE;
}


static gboolean
xfce_indicator_button_scroll (GtkWidget *widget, GdkEventScroll *event)
{
  XfceIndicatorButton *button = XFCE_INDICATOR_BUTTON (widget);

  g_signal_emit_by_name (button->io, INDICATOR_OBJECT_SIGNAL_ENTRY_SCROLLED,
                         button->entry, 1, event->direction);

  return TRUE;
}


static void
xfce_indicator_button_menu_deactivate (XfceIndicatorButton *button,
                                       GtkMenu             *menu)
{
  g_return_if_fail (XFCE_IS_INDICATOR_BUTTON (button));
  g_return_if_fail (GTK_IS_MENU (menu));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
}




/* When can_focus is true, GtkButton allocates larger size than requested *
 * and causes the panel image to grow indefinitely.                       *
 * This workaround compensates for this difference.                       *
 * Details in https://bugzilla.gnome.org/show_bug.cgi?id=698030           *
 */
static gint
xfce_indicator_button_padding_correction (GtkWidget *widget)
{
  GtkStyleContext       *context;
  gint                   focus_width;
  gint                   focus_pad;
  gint                   correction;

  if (!gtk_widget_get_can_focus (widget))
    {
      context = gtk_widget_get_style_context (widget);
      gtk_style_context_get_style (context,
                                   "focus-line-width", &focus_width,
                                   "focus-padding", &focus_pad,
                                   NULL);
      correction = (focus_width + focus_pad) * 2;
    }
  else
    {
      correction = 0;
    }

  return correction;
}


static void
xfce_indicator_button_get_preferred_width (GtkWidget *widget,
                                           gint      *minimum_width,
                                           gint      *natural_width)
{
  gint                 correction;

  g_return_if_fail (XFCE_IS_INDICATOR_BUTTON (widget));

  (*GTK_WIDGET_CLASS (xfce_indicator_button_parent_class)->get_preferred_width) (widget, minimum_width, natural_width);

  correction = xfce_indicator_button_padding_correction (widget);

  if (minimum_width != NULL)
    *minimum_width = MAX (1, *minimum_width - correction);

  if (natural_width != NULL)
    *natural_width = MAX (1, *natural_width - correction);
}



static void
xfce_indicator_button_get_preferred_height (GtkWidget *widget,
                                            gint      *minimum_height,
                                            gint      *natural_height)
{
  gint                 correction;

  g_return_if_fail (XFCE_IS_INDICATOR_BUTTON (widget));

  (*GTK_WIDGET_CLASS (xfce_indicator_button_parent_class)->get_preferred_height) (widget, minimum_height, natural_height);

  correction = xfce_indicator_button_padding_correction (widget);

  if (minimum_height != NULL)
    *minimum_height = MAX (1, *minimum_height - correction);

  if (natural_height != NULL)
    *natural_height = MAX (1, *natural_height - correction);
}
