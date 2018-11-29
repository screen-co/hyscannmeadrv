/* nmea-uart-test.c
 *
 * Copyright 2016-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
 *
 * This file is part of HyScanNMEADrv.
 *
 * HyScanNMEADrv is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanNMEADrv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanNMEADrv имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanNMEADrv на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#include <hyscan-nmea-uart.h>
#include <stdio.h>

void
data_cb (HyScanNmeaUART *uart,
         gint64          time,
         const gchar    *nmea,
         guint           size,
         gpointer        user_data)
{
  gdouble dtime = time / 1000000.0;
  g_print ("%s: rx time %.03fs\n%s\n", (gchar*)user_data, dtime, nmea);
}

int
main (int    argc,
      char **argv)
{
  gboolean list = FALSE;
  GList *devices = NULL;
  GList *uarts = NULL;
  GList *link;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "list", 'l', 0, G_OPTION_ARG_NONE, &list, "List available UART ports", NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    g_option_context_free (context);
    g_strfreev (args);
  }

  devices = hyscan_nmea_uart_list_devices ();
  if (devices == NULL)
    {
      g_print ("No uart devices found.\n");
      return -1;
    }

  if (list)
    g_print ("UART ports:\n");

  link = devices;
  while (link != NULL)
    {
      HyScanNmeaUARTDevice *device = link->data;
      HyScanNmeaUART *uart = hyscan_nmea_uart_new ();

      if (list)
        {
          g_print ("  %s: %s\n", device->name, device->path);
        }
      else
        {
          g_signal_connect (uart, "nmea-data", G_CALLBACK (data_cb), (gpointer)device->name);
          hyscan_nmea_uart_set_device (uart, device->path, HYSCAN_NMEA_UART_MODE_AUTO);
        }

      uarts = g_list_prepend (uarts, uart);
      link = g_list_next (link);
    }

  if (!list)
    {
      g_print ("Press [Enter] to terminate test...\n");
      getchar ();
    }

  g_list_free_full (uarts, g_object_unref);
  g_list_free_full (devices, (GDestroyNotify)hyscan_nmea_uart_device_free);

  return 0;
}
