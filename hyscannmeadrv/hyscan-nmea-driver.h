/* hyscan-nmea-driver.h
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

#ifndef __HYSCAN_NMEA_DRIVER_H__
#define __HYSCAN_NMEA_DRIVER_H__

#include <hyscan-param.h>
#include <hyscan-sensor.h>
#include <hyscan-param-list.h>

G_BEGIN_DECLS

#define HYSCAN_NMEA_DRIVER_UART_URI         "nmea://uart"
#define HYSCAN_NMEA_DRIVER_UDP_URI          "nmea://udp"

#define HYSCAN_TYPE_NMEA_DRIVER             (hyscan_nmea_driver_get_type ())
#define HYSCAN_NMEA_DRIVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NMEA_DRIVER, HyScanNmeaDriver))
#define HYSCAN_IS_NMEA_DRIVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NMEA_DRIVER))
#define HYSCAN_NMEA_DRIVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NMEA_DRIVER, HyScanNmeaDriverClass))
#define HYSCAN_IS_NMEA_DRIVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NMEA_DRIVER))
#define HYSCAN_NMEA_DRIVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NMEA_DRIVER, HyScanNmeaDriverClass))

typedef struct _HyScanNmeaDriver HyScanNmeaDriver;
typedef struct _HyScanNmeaDriverPrivate HyScanNmeaDriverPrivate;
typedef struct _HyScanNmeaDriverClass HyScanNmeaDriverClass;

struct _HyScanNmeaDriver
{
  GObject parent_instance;

  HyScanNmeaDriverPrivate *priv;
};

struct _HyScanNmeaDriverClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_nmea_driver_get_type             (void);

HYSCAN_API
HyScanNmeaDriver *     hyscan_nmea_driver_new                  (const gchar           *uri,
                                                                HyScanParamList       *params);

HYSCAN_API
HyScanDataSchema *     hyscan_nmea_driver_get_connect_schema   (const gchar           *uri);

HYSCAN_API
gboolean               hyscan_nmea_driver_check_connect        (const gchar           *uri,
                                                                HyScanParamList       *params);

G_END_DECLS

#endif /* __HYSCAN_NMEA_DRIVER_H__ */
