/* hyscan-nmea-uart.h
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

#ifndef __HYSCAN_NMEA_UART_H__
#define __HYSCAN_NMEA_UART_H__

#include <hyscan-nmea-receiver.h>

G_BEGIN_DECLS

/**
 * HyScanNmeaUARTMode:
 * @HYSCAN_NMEA_UART_MODE_DISABLED: Порт отключен.
 * @HYSCAN_NMEA_UART_MODE_AUTO: Автоматический выбор режима работы.
 * @HYSCAN_NMEA_UART_MODE_4800_8N1: Скорость 4800 бод, 8N1.
 * @HYSCAN_NMEA_UART_MODE_9600_8N1: Скорость 9600 бод, 8N1.
 * @HYSCAN_NMEA_UART_MODE_19200_8N1: Скорость 19200 бод, 8N1.
 * @HYSCAN_NMEA_UART_MODE_38400_8N1: Скорость 38400 бод, 8N1.
 * @HYSCAN_NMEA_UART_MODE_57600_8N1: Скорость 57600 бод, 8N1.
 * @HYSCAN_NMEA_UART_MODE_115200_8N1: Скорость 1152000 бод, 8N1.
 *
 * Режимы работы UART порта.
 */
typedef enum
{
  HYSCAN_NMEA_UART_MODE_DISABLED,
  HYSCAN_NMEA_UART_MODE_AUTO,
  HYSCAN_NMEA_UART_MODE_4800_8N1,
  HYSCAN_NMEA_UART_MODE_9600_8N1,
  HYSCAN_NMEA_UART_MODE_19200_8N1,
  HYSCAN_NMEA_UART_MODE_38400_8N1,
  HYSCAN_NMEA_UART_MODE_57600_8N1,
  HYSCAN_NMEA_UART_MODE_115200_8N1
} HyScanNmeaUARTMode;

/**
 * HyScanNmeaUARTDevice:
 * @name: название UART порта
 * @path: путь к файлу устройства порта
 *
 * Описание UART порта.
 */
typedef struct
{
  const gchar                 *name;
  const gchar                 *path;
} HyScanNmeaUARTDevice;

#define HYSCAN_TYPE_NMEA_UART            (hyscan_nmea_uart_get_type ())
#define HYSCAN_NMEA_UART(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NMEA_UART, HyScanNmeaUART))
#define HYSCAN_IS_UART(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NMEA_UART))
#define HYSCAN_NMEA_UART_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NMEA_UART, HyScanNmeaUARTClass))
#define HYSCAN_IS_UART_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NMEA_UART))
#define HYSCAN_NMEA_UART_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NMEA_UART, HyScanNmeaUARTClass))

typedef struct _HyScanNmeaUART HyScanNmeaUART;
typedef struct _HyScanNmeaUARTPrivate HyScanNmeaUARTPrivate;
typedef struct _HyScanNmeaUARTClass HyScanNmeaUARTClass;

struct _HyScanNmeaUART
{
  HyScanNmeaReceiver parent_instance;

  HyScanNmeaUARTPrivate *priv;
};

struct _HyScanNmeaUARTClass
{
  HyScanNmeaReceiverClass parent_class;
};

HYSCAN_API
GType                  hyscan_nmea_uart_device_get_type  (void);

HYSCAN_API
GType                  hyscan_nmea_uart_get_type         (void);

HYSCAN_API
HyScanNmeaUART *       hyscan_nmea_uart_new              (void);

HYSCAN_API
gboolean               hyscan_nmea_uart_set_device       (HyScanNmeaUART                *uart,
                                                          const gchar                   *path,
                                                          HyScanNmeaUARTMode             mode);

HYSCAN_API
GList *                hyscan_nmea_uart_list_devices     (void);

HYSCAN_API
HyScanNmeaUARTDevice * hyscan_nmea_uart_device_copy      (const HyScanNmeaUARTDevice    *devices);

HYSCAN_API
void                   hyscan_nmea_uart_device_free      (HyScanNmeaUARTDevice          *devices);

G_END_DECLS

#endif /* __HYSCAN_NMEA_UART_H__ */
