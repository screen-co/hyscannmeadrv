/* nmea-drv-test.c
 *
 * Copyright 2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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

#include <hyscan-driver.h>
#include <hyscan-sensor.h>
#include <hyscan-param.h>
#include <hyscan-buffer.h>
#include <hyscan-nmea-driver.h>

#include <libxml/parser.h>
#include <stdio.h>
#include <math.h>

gboolean shutdown = FALSE;

/* Поток контроля состояния датчика. */
gpointer
status_check (gpointer data)
{
  HyScanParam *param = data;
  HyScanDataSchema *schema = hyscan_param_schema (param);
  HyScanParamList *list = hyscan_param_list_new ();

  const gchar * const *keys = hyscan_data_schema_list_keys (schema);
  const gchar *status_id = NULL;

  guint i;

  for (i = 0; (keys != NULL) && (keys[i] != NULL); i++)
    {
      if (g_str_has_prefix (keys[i], "/state/") && g_str_has_suffix (keys[i], "/status"))
        status_id = keys[i];
    }

  if (status_id == NULL)
    goto exit;

  while (!g_atomic_int_get (&shutdown))
    {
      hyscan_param_list_clear (list);
      hyscan_param_list_add (list, status_id);

      if (hyscan_param_get (param, list))
        g_print ("Sensor status: %s\n", hyscan_param_list_get_string (list, status_id));

      g_usleep (1000000);
    }

exit:
  g_object_unref (schema);
  g_object_unref (list);

  return NULL;
}

/* Данные от датчика. */
void
nmea_cb (HyScanDriver *nmea,
         const gchar  *name,
         gint          source,
         gint64        time,
         HyScanBuffer *buffer,
         gpointer      user_data)
{
  const gchar *str;
  guint32 size;

  if (source != HYSCAN_SOURCE_NMEA_ANY)
    return;

  str = hyscan_buffer_get_data (buffer, &size);
  g_print ("Data from %s\n%s\n", name, str);
}

/* Сообщения от драйвера датчика. */
void
log_cb (HyScanDriver   *nmea,
        const gchar    *name,
        gint64          time,
        HyScanLogLevel  log_level,
        const gchar    *message,
        gpointer        user_data)
{
  g_print ("Log message: %s\n", message);
}

int
main (int    argc,
      char **argv)
{
  gchar *path = NULL;
  gboolean show_info = FALSE;
  gboolean list_sensors = FALSE;
  gchar *uri = NULL;
  gchar *uart_port = NULL;
  gchar *uart_mode = NULL;
  gchar *udp_address = NULL;
  gint udp_port = 0;
  gchar *URI = NULL;

  HyScanDriver *driver;
  HyScanDataSchema *connect;
  HyScanParamList *params;
  GThread *status_thread;
  HyScanDevice *nmea;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "path", 'a', 0, G_OPTION_ARG_STRING, &path, "Driver path (default: current directory)", NULL },
        { "info", 'i', 0, G_OPTION_ARG_NONE, &show_info, "Show driver info", NULL },
        { "list", 'l', 0, G_OPTION_ARG_NONE, &list_sensors, "List sensors and parameters", NULL },
        { "uri", 'u', 0, G_OPTION_ARG_STRING, &uri, "Sensor uri", NULL },
        { "uart-port", 'o', 0, G_OPTION_ARG_STRING, &uart_port, "UART port", NULL },
        { "uart-mode", 'm', 0, G_OPTION_ARG_STRING, &uart_mode, "UART mode", NULL },
        { "udp-address", 'h', 0, G_OPTION_ARG_STRING, &udp_address, "UDP address", NULL },
        { "udp-port", 'p', 0, G_OPTION_ARG_INT, &udp_port, "UDP port", NULL },
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

    if ((!show_info && !list_sensors && (uri == NULL)) ||
        ((udp_port != 0) && (udp_port < 1024)) ||
        ((udp_port != 0) && (udp_port > 65535)))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
    g_strfreev (args);
  }

  /* По умолчанию грузим драйвер из текущего каталога. */
  if (path == NULL)
    path = g_strdup (".");

  /* Загрузка драйвера */
  driver = hyscan_driver_new (path, "nmea");
  if (driver == NULL)
    g_error ("can't load nmea driver");

  /* Информация о драйвере. */
  if (show_info)
    {
      HyScanDataSchema *info;
      const gchar * const *keys;
      guint i;

      info = hyscan_driver_get_info (".", "nmea");
      keys = hyscan_data_schema_list_keys (info);

      g_print ("*** Driver info ***\n");
      for (i = 0; keys[i] != NULL; i++)
        {
          const gchar *name;
          GVariant *value;

          if (!g_str_has_prefix (keys[i], "/info/"))
            continue;

          name = hyscan_data_schema_key_get_name (info, keys[i]);
          value = hyscan_data_schema_key_get_default (info, keys[i]);
          g_print ("%s: %s\n", name, g_variant_get_string (value, NULL));

          g_variant_unref (value);
        }
      g_print ("\n");

      g_object_unref (info);

      goto exit;
    }

  /* Список датчиков и параметры подключения. */
  if (list_sensors)
    {
      GList *sensors, *sensor;

      sensor = sensors = hyscan_discover_list (HYSCAN_DISCOVER (driver));

      while (sensor != NULL)
        {
          HyScanDiscoverInfo *info = sensor->data;

          connect = hyscan_discover_config (HYSCAN_DISCOVER (driver), info->uri);

          g_print ("%s\n  uri: %s\n", info->name, info->uri);

          /* Параметры UART датчика. */
          if (g_strcmp0 (info->uri, HYSCAN_NMEA_DRIVER_UART_URI) == 0)
            {
              const gchar *port_enum_id;
              const gchar *mode_enum_id;
              GList *ports, *port;
              GList *modes, *mode;

              /* Список портов. */
              port_enum_id = hyscan_data_schema_key_get_enum_id (connect, "/uart/port");
              ports = port = hyscan_data_schema_get_enum_values (connect, port_enum_id);

              if (ports != NULL)
                g_print ("  ports:");

              while (port != NULL)
                {
                  HyScanDataSchemaEnumValue *value = port->data;

                  g_print (" %s", value->name);

                  port = g_list_next (port);

                  if (port != NULL)
                    g_print (",");
                }

              if (ports != NULL)
                g_print ("\n");

              g_list_free (ports);

              /* Список режимов работы. */
              mode_enum_id = hyscan_data_schema_key_get_enum_id (connect, "/uart/mode");
              modes = mode = hyscan_data_schema_get_enum_values (connect, mode_enum_id);

              if (modes != NULL)
                g_print ("  modes:");

              while (mode != NULL)
                {
                  HyScanDataSchemaEnumValue *value = mode->data;

                  g_print (" %s", value->name);

                  mode = g_list_next (mode);

                  if (mode != NULL)
                    g_print (",");
                }

              if (modes != NULL)
                g_print ("\n");

              g_list_free (modes);
            }

          /* Параметры UDP датчика. */
          if (g_strcmp0 (info->uri, HYSCAN_NMEA_DRIVER_UDP_URI) == 0)
            {
              const gchar *address_enum_id;
              GList *addresses, *address;

              /* Список адресов. */
              address_enum_id = hyscan_data_schema_key_get_enum_id (connect, "/udp/address");
              addresses = address = hyscan_data_schema_get_enum_values (connect, address_enum_id);

              if (addresses != NULL)
                g_print ("  addresses:");

              while (address != NULL)
                {
                  HyScanDataSchemaEnumValue *value = address->data;

                  g_print (" %s", value->name);

                  address = g_list_next (address);

                  if (address != NULL)
                    g_print (",");
                }

              if (addresses != NULL)
                g_print ("\n");

              g_list_free (addresses);

              g_print ("  port: 1024 - 65535\n");
            }

          g_object_unref (connect);
          sensor = g_list_next (sensor);
        }

      g_list_free_full (sensors, (GDestroyNotify)hyscan_discover_info_free);

      goto exit;
    }

  /* Параметры подключения. */
  URI = g_ascii_strdown (uri, -1);
  params = hyscan_param_list_new ();
  connect = hyscan_discover_config (HYSCAN_DISCOVER (driver), uri);
  if (connect == NULL)
    g_error ("Unknown sensor uri %s", uri);

  /* Параметры подключения UART датчика. */
  if (g_strcmp0 (URI, HYSCAN_NMEA_DRIVER_UART_URI) == 0)
    {
      const gchar *port_enum_id;
      const gchar *mode_enum_id;
      GList *ports, *port;
      GList *modes, *mode;

      /* Ищем идентификатор порта. */
      port_enum_id = hyscan_data_schema_key_get_enum_id (connect, "/uart/port");
      ports = port = hyscan_data_schema_get_enum_values (connect, port_enum_id);
      while (port != NULL)
        {
          HyScanDataSchemaEnumValue *value = port->data;

          if (g_strcmp0 (value->name, uart_port) == 0)
            hyscan_param_list_set_enum (params, "/uart/port", value->value);

          port = g_list_next (port);
        }
      g_list_free (ports);

      /* Ищем идентификатор режима работы. */
      mode_enum_id = hyscan_data_schema_key_get_enum_id (connect, "/uart/mode");
      modes = mode = hyscan_data_schema_get_enum_values (connect, mode_enum_id);
      while (mode != NULL)
        {
          HyScanDataSchemaEnumValue *value = mode->data;

          if (g_strcmp0 (value->name, uart_mode) == 0 )
            hyscan_param_list_set_enum (params, "/uart/mode", value->value);

          mode = g_list_next (mode);
        }
      g_list_free (modes);
    }

  /* Параметры UDP датчика. */
  if (g_strcmp0 (URI, HYSCAN_NMEA_DRIVER_UDP_URI) == 0)
    {
      const gchar *address_enum_id;
      GList *addresses, *address;

      /* Список адресов. */
      address_enum_id = hyscan_data_schema_key_get_enum_id (connect, "/udp/address");
      addresses = address = hyscan_data_schema_get_enum_values (connect, address_enum_id);
      while (address != NULL)
        {
          HyScanDataSchemaEnumValue *value = address->data;

          if (g_strcmp0 (value->name, udp_address) == 0)
            hyscan_param_list_set_enum (params, "/udp/address", value->value);

          address = g_list_next (address);
        }
      g_list_free (addresses);

      if (udp_port != 0)
        hyscan_param_list_set_integer (params, "/udp/port", udp_port);
    }

  /* Проверяем параметры подключения к датчику. */
  if (!hyscan_discover_check (HYSCAN_DISCOVER (driver), uri, params))
    g_error ("Unknown sensor uri %s", uri);

  /* Подключение к датчику. */
  nmea = hyscan_discover_connect (HYSCAN_DISCOVER (driver), uri, params);
  g_signal_connect (nmea, "sensor-data", G_CALLBACK (nmea_cb), NULL);
  g_signal_connect (nmea, "sensor-log", G_CALLBACK (log_cb), NULL);

  status_thread = g_thread_new ("status", status_check, nmea);

  g_print ("Press [Enter] to terminate test...\n");
  getchar ();

  shutdown = TRUE;
  g_thread_join (status_thread);

  g_object_unref (connect);
  g_object_unref (params);
  g_object_unref (nmea);

exit:
  g_free (path);
  g_free (URI);
  g_free (uri);
  g_free (uart_port);
  g_free (uart_mode);
  g_free (udp_address);
  g_object_unref (driver);
  xmlCleanupParser ();

  return 0;
}
