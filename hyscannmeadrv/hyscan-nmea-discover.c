/* hyscan-nmea-discover.c
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

/**
 * SECTION: hyscan-nmea-discover
 * @Short_description: класс подключения к NMEA датчикам
 * @Title: HyScanNmeaDiscover
 *
 * Класс реализует интерфейс #HyScanDiscover и предназначен для организации
 * подключения к NMEA датчикам. Этот класс предназначен для использования
 * драйвером NMEA датчика.
 */

#include <hyscan-data-schema-builder.h>

#include "hyscan-nmea-discover.h"
#include "hyscan-nmea-driver.h"
#include "hyscan-nmea-uart.h"
#include "hyscan-nmea-udp.h"
#include "hyscan-nmea-drv.h"

#include <glib/gi18n-lib.h>

static void    hyscan_nmea_discover_interface_init     (HyScanDiscoverInterface *iface);

G_DEFINE_TYPE_WITH_CODE (HyScanNmeaDiscover, hyscan_nmea_discover, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DISCOVER, hyscan_nmea_discover_interface_init))

static void
hyscan_nmea_discover_class_init (HyScanNmeaDiscoverClass *klass)
{
}

static void
hyscan_nmea_discover_init (HyScanNmeaDiscover *nmea)
{
}

static HyScanDataSchema *
hyscan_nmea_discover_info_schema (void)
{
  HyScanDataSchemaBuilder *builder;
  HyScanDataSchema *info;

  builder = hyscan_data_schema_builder_new ("driver-info");

  hyscan_data_schema_builder_key_string_create  (builder, "/name",
                                                 _("Name"), NULL,
                                                 _("NMEA 0183 compatible device"));
  hyscan_data_schema_builder_key_set_access     (builder, "/name",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  hyscan_data_schema_builder_key_string_create  (builder, "/version",
                                                 _("Driver version"), NULL,
                                                 HYSCAN_NMEA_DRIVER_VERSION);
  hyscan_data_schema_builder_key_set_access     (builder, "/version",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  info = hyscan_data_schema_builder_get_schema (builder);

  g_object_unref (builder);

  return info;
}

static void
hyscan_nmea_discover_start (HyScanDiscover *discover)
{
  g_signal_emit_by_name (discover, "progress", 100.0);
  g_signal_emit_by_name (discover, "completed");
}

static GList *
hyscan_nmea_discover_list (HyScanDiscover *discover)
{
  GList *uris = NULL;
  HyScanDiscoverInfo *info;
  HyScanDataSchema *schema;

  schema = hyscan_nmea_discover_info_schema ();

  info = hyscan_discover_info_new (_("UDP NMEA sensor"),
                                   schema,
                                   HYSCAN_NMEA_DRIVER_UDP_URI,
                                   TRUE);
  uris = g_list_prepend (uris, info);

  info = hyscan_discover_info_new (_("UART NMEA sensor"),
                                   schema,
                                   HYSCAN_NMEA_DRIVER_UART_URI,
                                   TRUE);
  uris = g_list_prepend (uris, info);

  g_object_unref (schema);

  return uris;
}

static HyScanDataSchema *
hyscan_nmea_discover_config (HyScanDiscover *discover,
                             const gchar    *uri)
{
  return hyscan_nmea_driver_get_connect_schema (uri);
}

static gboolean
hyscan_nmea_discover_check (HyScanDiscover  *discover,
                            const gchar     *uri,
                            HyScanParamList *params)
{
  return hyscan_nmea_driver_check_connect (uri, params);
}

static HyScanDevice *
hyscan_nmea_discover_connect (HyScanDiscover  *discover,
                              const gchar     *uri,
                              HyScanParamList *params)
{
  return HYSCAN_DEVICE (hyscan_nmea_driver_new (uri, params));
}

/**
 * hyscan_nmea_discover_new:
 *
 * Функция создаёт новый объект #HyScanNmeaDiscover.
 *
 * Returns: #HyScanNmeaDiscover. Для удаления #g_object_unref.
 */
HyScanNmeaDiscover *
hyscan_nmea_discover_new (void)
{
  return g_object_new (HYSCAN_TYPE_NMEA_DISCOVER, NULL);
}

static void
hyscan_nmea_discover_interface_init (HyScanDiscoverInterface *iface)
{
  iface->start = hyscan_nmea_discover_start;
  iface->stop = NULL;
  iface->list = hyscan_nmea_discover_list;
  iface->config = hyscan_nmea_discover_config;
  iface->check = hyscan_nmea_discover_check;
  iface->connect = hyscan_nmea_discover_connect;
}
