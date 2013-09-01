/*  Copyright (c) 2009      Mark Trompell <mark@foresightlinux.org>
 *  Copyright (c) 2012-2013 Andrzej <ndrwrdck@gmail.com>
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
 *  This file implements the main plugin class.
 *
 */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libindicator/indicator-object.h>
#include <libido/libido.h>
#include <libindicator/indicator-ng.h>

#include "indicator.h"
#include "indicator-box.h"
#include "indicator-button.h"
#include "indicator-dialog.h"

#ifdef LIBXFCE4PANEL_CHECK_VERSION
#if LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
#define HAS_PANEL_49
#endif
#endif

/* prototypes */
static void             indicator_construct                        (XfcePanelPlugin       *plugin);
static void             indicator_free                             (XfcePanelPlugin       *plugin);
static gboolean         load_module                                (const gchar           *name,
                                                                    IndicatorPlugin       *indicator);
static void             load_indicator                             (const gchar           *name,
                                                                    IndicatorPlugin       *indicator,
                                                                    IndicatorObject *io);
static void             load_modules                               (IndicatorPlugin       *indicator,
                                                                    gint *indicators_loaded);
static void             load_services                              (IndicatorPlugin *indicatorplugin,
                                                                    gint *indicators_loaded);
static void             indicator_show_about                       (XfcePanelPlugin       *plugin);
static void             indicator_configure_plugin                 (XfcePanelPlugin       *plugin);
static gboolean         indicator_size_changed                     (XfcePanelPlugin       *plugin,
                                                                    gint                   size);
#ifdef HAS_PANEL_49
static void             indicator_mode_changed                     (XfcePanelPlugin       *plugin,
                                                                    XfcePanelPluginMode    mode);
#else
static void             indicator_orientation_changed              (XfcePanelPlugin       *plugin,
                                                                    GtkOrientation         orientation);
#endif


struct _IndicatorPluginClass
{
  XfcePanelPluginClass __parent__;
};

/* plugin structure */
struct _IndicatorPlugin
{
  XfcePanelPlugin __parent__;

  /* panel widgets */
  GtkWidget       *item;
  GtkWidget       *buttonbox;

  /* indicator settings */
  IndicatorConfig *config;

  /* log file */
  FILE            *logfile;
};


/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN (IndicatorPlugin, indicator)




static void
indicator_class_init (IndicatorPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = indicator_construct;
  plugin_class->free_data = indicator_free;
  plugin_class->size_changed = indicator_size_changed;
  plugin_class->about = indicator_show_about;
  plugin_class->configure_plugin = indicator_configure_plugin;
#ifdef HAS_PANEL_49
  plugin_class->mode_changed = indicator_mode_changed;
#else
  plugin_class->orientation_changed = indicator_orientation_changed;
#endif
}



static void
indicator_init (IndicatorPlugin *indicator)
{
  /* Indicators print a lot of warnings. By default, "wrapper"
     makes them critical, so the plugin "crashes" when run as an external
     plugin but not internal one (loaded by "xfce4-panel" itself).
     The following lines makes only g_error critical. */
  g_log_set_always_fatal (G_LOG_LEVEL_ERROR);

  indicator->item      = NULL;
  indicator->buttonbox = NULL;
  indicator->config    = NULL;
  indicator->logfile   = NULL;
}



static void
indicator_free (XfcePanelPlugin *plugin)
{
  GtkWidget *dialog;

  /* check if the dialog is still open. if so, destroy it */
  dialog = g_object_get_data (G_OBJECT (plugin), "dialog");
  if (G_UNLIKELY (dialog != NULL))
    gtk_widget_destroy (dialog);
}



static void
indicator_show_about (XfcePanelPlugin *plugin)
{
   GdkPixbuf *icon;

   const gchar *auth[] = {
     "Mark Trompell <mark@foresightlinux.org>", "Andrzej Radecki <ndrwrdck@gmail.com>",
     "Lionel Le Folgoc <lionel@lefolgoc.net>", "Jason Conti <jconti@launchpad.net>",
     "Nick Schermer <nick@xfce.org>", "Evgeni Golov <evgeni@debian.org>", NULL };

   g_return_if_fail (XFCE_IS_INDICATOR_PLUGIN (plugin));

   icon = xfce_panel_pixbuf_from_source("xfce4-indicator-plugin", NULL, 32);
   gtk_show_about_dialog(NULL,
                         "logo", icon,
                         "license", xfce_get_license_text (XFCE_LICENSE_TEXT_GPL),
                         "version", PACKAGE_VERSION,
                         "program-name", PACKAGE_NAME,
                         "comments", _("An indicator of something that needs your attention on the desktop"),
                         "website", "http://goodies.xfce.org/projects/panel-plugins/xfce4-indicator-plugin",
                         "copyright", _("Copyright (c) 2009-2013\n"),
                         "authors", auth, NULL);

   if(icon)
     g_object_unref(G_OBJECT(icon));
}



static void
indicator_configure_plugin (XfcePanelPlugin *plugin)
{
  IndicatorPlugin *indicator = XFCE_INDICATOR_PLUGIN (plugin);

  indicator_dialog_show (gtk_widget_get_screen (GTK_WIDGET (plugin)), indicator->config);
}




#ifdef HAS_PANEL_49
static void
indicator_mode_changed (XfcePanelPlugin     *plugin,
                        XfcePanelPluginMode  mode)
{
  GtkOrientation   orientation;
  GtkOrientation   panel_orientation = xfce_panel_plugin_get_orientation (plugin);
  IndicatorPlugin *indicator = XFCE_INDICATOR_PLUGIN (plugin);

  orientation = (mode == XFCE_PANEL_PLUGIN_MODE_VERTICAL) ?
    GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;

  indicator_config_set_orientation (indicator->config, panel_orientation, orientation);

  indicator_size_changed (plugin, xfce_panel_plugin_get_size (plugin));
}



#else
static void
indicator_orientation_changed (XfcePanelPlugin *plugin,
                               GtkOrientation   orientation)
{
  IndicatorPlugin *indicator = XFCE_INDICATOR_PLUGIN (plugin);

  indicator_config_set_orientation (indicator->config, orientation, GTK_ORIENTATION_HORIZONTAL);

  indicator_size_changed (plugin, xfce_panel_plugin_get_size (plugin));
}
#endif


static gboolean
indicator_size_changed (XfcePanelPlugin *plugin,
                        gint             size)
{
  IndicatorPlugin *indicator = XFCE_INDICATOR_PLUGIN (plugin);

#ifdef HAS_PANEL_49
  indicator_config_set_size (indicator->config, size, xfce_panel_plugin_get_nrows (plugin));
#else
  indicator_config_set_size (indicator->config, size, 1);
#endif

  return TRUE;
}



static void
indicator_log_handler (const gchar    *domain,
                       GLogLevelFlags  level,
                       const gchar    *message,
                       gpointer        data)
{
  IndicatorPlugin *indicator = XFCE_INDICATOR_PLUGIN (data);
  gchar           *path;
  const gchar     *prefix;

  if (indicator->logfile == NULL)
    {
      g_mkdir_with_parents (g_get_user_cache_dir (), 0755);
      path = g_build_filename (g_get_user_cache_dir (), "xfce4-indicator-plugin.log", NULL);
      indicator->logfile = fopen (path, "w");
      g_free (path);
    }

  if (indicator->logfile)
    {
      switch (level & G_LOG_LEVEL_MASK)
        {
        case G_LOG_LEVEL_ERROR:    prefix = "ERROR";    break;
        case G_LOG_LEVEL_CRITICAL: prefix = "CRITICAL"; break;
        case G_LOG_LEVEL_WARNING:  prefix = "WARNING";  break;
        case G_LOG_LEVEL_MESSAGE:  prefix = "MESSAGE";  break;
        case G_LOG_LEVEL_INFO:     prefix = "INFO";     break;
        case G_LOG_LEVEL_DEBUG:    prefix = "DEBUG";    break;
        default:                   prefix = "LOG";      break;
        }

      fprintf (indicator->logfile, "%-10s %-25s %s\n", prefix, domain, message);
      fflush (indicator->logfile);
    }

  /* print log to the stdout */
  if (level & G_LOG_LEVEL_ERROR || level & G_LOG_LEVEL_CRITICAL)
    g_log_default_handler (domain, level, message, NULL);
}



static void
indicator_construct (XfcePanelPlugin *plugin)
{
  IndicatorPlugin  *indicator = XFCE_INDICATOR_PLUGIN (plugin);
  gint              indicators_loaded = 0;
  GtkWidget        *label;

  xfce_panel_plugin_menu_show_configure (plugin);
  xfce_panel_plugin_menu_show_about (plugin);

  /* setup transation domain */
  xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* log messages to a file */
  g_log_set_default_handler(indicator_log_handler, plugin);

  /* Init some theme/icon stuff */
  gtk_icon_theme_append_search_path(gtk_icon_theme_get_default(),
                                  INDICATOR_ICONS_DIR);
  /*gtk_widget_set_name(GTK_WIDGET (indicator->plugin), "indicator-plugin");*/

  /* initialize xfconf */
  indicator->config = indicator_config_new (xfce_panel_plugin_get_property_base (plugin));

  /* instantiate a button box */
  indicator->buttonbox = xfce_indicator_box_new (indicator->config);
  gtk_container_add (GTK_CONTAINER (plugin), GTK_WIDGET(indicator->buttonbox));
  gtk_widget_show(GTK_WIDGET(indicator->buttonbox));

  /* load 'em */
  load_modules(indicator, &indicators_loaded);
  load_services(indicator, &indicators_loaded);

  if (indicators_loaded == 0) {
    /* A label to allow for click through */
    indicator->item = xfce_indicator_button_new (NULL,
                                                 "<placeholder>",
                                                 NULL,
                                                 plugin,
                                                 indicator->config);
    label = gtk_label_new ( _("No Indicators"));
    xfce_indicator_button_set_label (XFCE_INDICATOR_BUTTON (indicator->item), GTK_LABEL (label));
    gtk_container_add (GTK_CONTAINER (indicator->buttonbox), indicator->item);
    gtk_widget_show (label);
    gtk_widget_show (indicator->item);
  }
}


static void
entry_added (IndicatorObject * io, IndicatorObjectEntry * entry, gpointer user_data)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (user_data);
  IndicatorPlugin *indicator = XFCE_INDICATOR_PLUGIN (plugin);
  const gchar     *io_name = g_object_get_data (G_OBJECT (io), "io-name");
  GtkWidget       *button = xfce_indicator_button_new (io,
                                                       io_name,
                                                       entry,
                                                       plugin,
                                                       indicator->config);

  /* remove placeholder item when there are real entries to be added */
  if (indicator->item != NULL)
    {
      xfce_indicator_button_disconnect_signals (XFCE_INDICATOR_BUTTON (indicator->item));
      gtk_widget_destroy (GTK_WIDGET (indicator->item));
      indicator->item = NULL;
    }

  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_button_set_use_underline(GTK_BUTTON (button),TRUE);
  gtk_widget_set_name(GTK_WIDGET (button), "indicator-button");

  if (entry->image != NULL)
    xfce_indicator_button_set_image(XFCE_INDICATOR_BUTTON(button), entry->image);

  if (entry->label != NULL)
    xfce_indicator_button_set_label(XFCE_INDICATOR_BUTTON(button), entry->label);

  if (entry->menu != NULL)
    xfce_indicator_button_set_menu (XFCE_INDICATOR_BUTTON(button), entry->menu);

  gtk_container_add(GTK_CONTAINER (indicator->buttonbox), button);
  gtk_widget_show(button);
}


static void
entry_removed (IndicatorObject * io, IndicatorObjectEntry * entry, gpointer user_data)
{
  xfce_indicator_box_remove_entry (XFCE_INDICATOR_BOX (user_data), entry);
}


static gboolean
load_module (const gchar * name, IndicatorPlugin * indicator)
{
  gchar                *fullpath;
  IndicatorObject      *io;

  g_debug("Looking at Module: %s", name);
  g_return_val_if_fail(name != NULL, FALSE);

  if (!g_str_has_suffix(name,G_MODULE_SUFFIX))
    return FALSE;

  g_debug("Loading Module: %s", name);

  fullpath = g_build_filename(INDICATOR_DIR, name, NULL);
  io = indicator_object_new_from_file(fullpath);
  g_free(fullpath);

  load_indicator(name, indicator, io);
  return TRUE;
}


/* FIXME: this dir can't currently be got from pkg-config */
#define INDICATOR_SERVICE_DIR "/usr/share/unity/indicators"

static gboolean
load_service (const gchar * name, IndicatorPlugin * indicator)
{
  gchar                *fullpath;
  IndicatorNg          *io;
  GError               *error = NULL;

  g_debug("Looking at Service: %s", name);
  g_return_val_if_fail(name != NULL, FALSE);

  g_debug("Loading Service: %s", name);

  fullpath = g_build_filename(INDICATOR_SERVICE_DIR, name, NULL);
  io = indicator_ng_new_for_profile (fullpath, "desktop", &error);
  g_free(fullpath);

  if (io)
    {
      load_indicator(name, indicator, INDICATOR_OBJECT(io));
      return TRUE;
    }
  else
    {
      g_clear_error (&error);
      return FALSE;
    }
}


static void
load_indicator(const gchar * name, IndicatorPlugin * indicator, IndicatorObject * io)
{
  GList                *entries, *entry;
  IndicatorObjectEntry *entrydata;

  indicator_config_add_known_indicator (indicator->config, name);

  g_object_set_data (G_OBJECT (io), "io-name", g_strdup (name));

  g_signal_connect(G_OBJECT(io), INDICATOR_OBJECT_SIGNAL_ENTRY_ADDED,
                   G_CALLBACK(entry_added), indicator);
  g_signal_connect(G_OBJECT(io), INDICATOR_OBJECT_SIGNAL_ENTRY_REMOVED,
                   G_CALLBACK(entry_removed), indicator->buttonbox);

  entries = indicator_object_get_entries(io);
  entry = NULL;

  for (entry = entries; entry != NULL; entry = g_list_next(entry))
    {
      entrydata = (IndicatorObjectEntry *)entry->data;
      entry_added(io, entrydata, indicator);
    }

  g_list_free(entries);
}


static void
load_modules(IndicatorPlugin * indicator, gint * indicators_loaded)
{
  if (g_file_test(INDICATOR_DIR, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {
    GDir * dir = g_dir_open(INDICATOR_DIR, 0, NULL);
    gint count = 0;
    const gchar * name;
    if (indicator_config_get_mode_whitelist (indicator->config))
      {
        while ((name = g_dir_read_name (dir)) != NULL)
          if (indicator_config_is_whitelisted (indicator->config, name))
            {
              g_debug ("Loading whitelisted module: %s", name);
              if (load_module(name, indicator))
                count++;
            }
      }
    else
      {
        while ((name = g_dir_read_name (dir)) != NULL)
          if (indicator_config_is_blacklisted (indicator->config, name))
            g_debug ("Excluding blacklisted module: %s", name);
          else if (load_module(name, indicator))
            count++;
      }

    g_dir_close (dir);

    *indicators_loaded += count;
  }
}


static void
load_services(IndicatorPlugin * indicator, gint * indicators_loaded)
{
  if (g_file_test(INDICATOR_SERVICE_DIR, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {
    GDir * dir = g_dir_open(INDICATOR_SERVICE_DIR, 0, NULL);
    gint count = 0;
    const gchar * name;
    if (indicator_config_get_mode_whitelist (indicator->config))
      {
        while ((name = g_dir_read_name (dir)) != NULL)
          if (indicator_config_is_whitelisted (indicator->config, name))
            {
              g_debug ("Loading whitelisted module: %s", name);
              if (load_service(name, indicator))
                count++;
            }
      }
    else
      {
        while ((name = g_dir_read_name (dir)) != NULL)
          if (indicator_config_is_blacklisted (indicator->config, name))
            g_debug ("Excluding blacklisted module: %s", name);
          else if (load_service(name, indicator))
            count++;
      }

    g_dir_close (dir);

    *indicators_loaded += count;
  }
}


XfceIndicatorBox *
indicator_get_buttonbox (IndicatorPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_INDICATOR_PLUGIN (plugin), NULL);

  return XFCE_INDICATOR_BOX (plugin->buttonbox);
}
