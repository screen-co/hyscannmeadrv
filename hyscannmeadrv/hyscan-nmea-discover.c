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
 * Contact the Screen LLC in this case - info@screen-co.ru
 */

/* HyScanNMEADrv имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanNMEADrv на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - info@screen-co.ru.
 */

/**
 * SECTION: hyscan-nmea-discover
 * @Short_description: класс подключения к NMEA датчикам
 * @Title: HyScanNmeaDiscoer
 *
 * Класс реализует интерфейс #HyScanDiscover и предназначен для организации
 * подключения к NMEA датчикам. Этот класс предназначен для использования
 * драйвером NMEA датчика.
 */

#include "hyscan-nmea-discover.h"
#include <hyscan-nmea-driver.h>
#include <hyscan-nmea-uart.h>
#include <hyscan-nmea-udp.h>

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

static GList *
hyscan_nmea_discover_list (HyScanDiscover *discover)
{
  GList *uris = NULL;
  HyScanDiscoverInfo *info;

  info = hyscan_discover_info_new (_ ("UDP NMEA sensor"),
                                   HYSCAN_NMEA_DRIVER_UDP_URI,
                                   TRUE);
  uris = g_list_prepend (uris, info);

  info = hyscan_discover_info_new (_ ("UART NMEA sensor"),
                                   HYSCAN_NMEA_DRIVER_UART_URI,
                                   TRUE);
  uris = g_list_prepend (uris, info);

  return uris;
}

static HyScanDataSchema *
hyscan_nmea_discover_config (HyScanDiscover *discover,
                             const gchar    *uri)
{
  return hyscan_nmea_driver_get_connect_schema (uri);
}

static gboolean
hyscan_nmea_discover_check (HyScanDiscover *discover,
                            const gchar    *uri)
{
  gchar *URI = g_ascii_strup (uri, -1);
  gboolean status = FALSE;

  if (g_strcmp0 (URI, HYSCAN_NMEA_DRIVER_UDP_URI) == 0)
    status = TRUE;
  if (g_strcmp0 (URI, HYSCAN_NMEA_DRIVER_UART_URI) == 0)
    status = TRUE;

  g_free (URI);

  return status;
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
  iface->start = NULL;
  iface->stop = NULL;
  iface->list = hyscan_nmea_discover_list;
  iface->config = hyscan_nmea_discover_config;
  iface->check = hyscan_nmea_discover_check;
  iface->connect = hyscan_nmea_discover_connect;
}
