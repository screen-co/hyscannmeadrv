/* hyscan-nmea-discover.h
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

#ifndef __HYSCAN_NMEA_DISCOVER_H__
#define __HYSCAN_NMEA_DISCOVER_H__

#include <hyscan-discover.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NMEA_DISCOVER             (hyscan_nmea_discover_get_type ())
#define HYSCAN_NMEA_DISCOVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NMEA_DISCOVER, HyScanNmeaDiscover))
#define HYSCAN_IS_NMEA_DISCOVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NMEA_DISCOVER))
#define HYSCAN_NMEA_DISCOVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NMEA_DISCOVER, HyScanNmeaDiscoverClass))
#define HYSCAN_IS_NMEA_DISCOVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NMEA_DISCOVER))
#define HYSCAN_NMEA_DISCOVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NMEA_DISCOVER, HyScanNmeaDiscoverClass))

typedef struct _HyScanNmeaDiscover HyScanNmeaDiscover;
typedef struct _HyScanNmeaDiscoverClass HyScanNmeaDiscoverClass;

struct _HyScanNmeaDiscover
{
  GObject parent_instance;
};

struct _HyScanNmeaDiscoverClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_nmea_discover_get_type           (void);

HYSCAN_API
HyScanNmeaDiscover *   hyscan_nmea_discover_new                (void);

G_END_DECLS

#endif /* __HYSCAN_NMEA_DISCOVER_H__ */
