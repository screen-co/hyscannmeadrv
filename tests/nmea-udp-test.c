/* nmea-udp-test.c
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

#include <hyscan-nmea-udp.h>
#include <stdio.h>

void
data_cb (HyScanNmeaUDP *udp,
         gint64         time,
         const gchar   *nmea,
         guint          size,
         gpointer       user_data)
{
  gdouble dtime = time / 1000000.0;
  g_print ("%s: rx time %.03fs\n%s\n", (gchar*)user_data, dtime, nmea);
}

int
main (int    argc,
      char **argv)
{
  HyScanNmeaUDP *udp;
  gboolean list = FALSE;
  gchar *host = NULL;
  gint port = 0;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "list", 'l', 0, G_OPTION_ARG_NONE, &list, "List available ip addresses", NULL },
        { "host", 'h', 0, G_OPTION_ARG_STRING, &host, "Bind ip address", NULL },
        { "port", 'p', 0, G_OPTION_ARG_INT, &port, "Bind udp port", NULL },
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

    if ((!list) && ((host == NULL) || (port < 1024) || (port > 65535)))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
    g_strfreev (args);
  }

  if (list)
    {
      gchar **addresses;
      guint i;

      addresses = hyscan_nmea_udp_list_addresses ();
      g_print ("Local ip addresses: \n");

      for (i = 0; addresses != NULL && addresses[i] != NULL; i++)
        g_print ("  %s\n", addresses[i]);

      g_strfreev (addresses);

      return 0;
    }

  udp = hyscan_nmea_udp_new ();
  hyscan_nmea_udp_set_address (udp, host, port);
  g_signal_connect (udp, "nmea-data", G_CALLBACK (data_cb), host);

  g_message ("Press [Enter] to terminate test...");
  getchar ();

  g_object_unref (udp);

  return 0;
}
