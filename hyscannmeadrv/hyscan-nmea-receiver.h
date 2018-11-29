/* hyscan-nmea-receiver.h
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

#ifndef __HYSCAN_NMEA_RECEIVER_H__
#define __HYSCAN_NMEA_RECEIVER_H__

#include <hyscan-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NMEA_RECEIVER             (hyscan_nmea_receiver_get_type ())
#define HYSCAN_NMEA_RECEIVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NMEA_RECEIVER, HyScanNmeaReceiver))
#define HYSCAN_IS_NMEA_RECEIVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NMEA_RECEIVER))
#define HYSCAN_NMEA_RECEIVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NMEA_RECEIVER, HyScanNmeaReceiverClass))
#define HYSCAN_IS_NMEA_RECEIVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NMEA_RECEIVER))
#define HYSCAN_NMEA_RECEIVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NMEA_RECEIVER, HyScanNmeaReceiverClass))

typedef struct _HyScanNmeaReceiver HyScanNmeaReceiver;
typedef struct _HyScanNmeaReceiverPrivate HyScanNmeaReceiverPrivate;
typedef struct _HyScanNmeaReceiverClass HyScanNmeaReceiverClass;

struct _HyScanNmeaReceiver
{
  GObject parent_instance;

  HyScanNmeaReceiverPrivate *priv;
};

struct _HyScanNmeaReceiverClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_nmea_receiver_get_type           (void);

HYSCAN_API
HyScanNmeaReceiver *   hyscan_nmea_receiver_new                (void);

HYSCAN_API
void                   hyscan_nmea_receiver_skip_broken        (HyScanNmeaReceiver      *receiver,
                                                                gboolean                 skip);

HYSCAN_API
gboolean               hyscan_nmea_receiver_add_data           (HyScanNmeaReceiver      *receiver,
                                                                gint64                   time,
                                                                const gchar             *data,
                                                                guint32                  size);

HYSCAN_API
void                   hyscan_nmea_receiver_flush              (HyScanNmeaReceiver      *receiver,
                                                                gdouble                  timeout);
HYSCAN_API
void                   hyscan_nmea_receiver_send_log           (HyScanNmeaReceiver      *receiver,
                                                                gint64                   time,
                                                                HyScanLogLevel           level,
                                                                const gchar             *message);

HYSCAN_API
void                   hyscan_nmea_receiver_io_error           (HyScanNmeaReceiver      *receiver);

G_END_DECLS

#endif /* __HYSCAN_NMEA_RECEIVER_H__ */
