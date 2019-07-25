/* hyscan-nmea-driver.c
 *
 * Copyright 2018-2019 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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
 * SECTION: hyscan-nmea-driver
 * @Short_description: класс драйвера NMEA датчика
 * @Title: HyScanNmeaDriver
 *
 * Класс реализует драйвер NMEA датчика обеспечивая приём данных через UART
 * порт или UDP порт. Класс реализует интерфейсы #HyScanSensor и #HyScanParam.
 * Выбор типа подключения осуществляется через путь к датчику - URI.
 * Для подключения через UART порт должен быть указан путь nmea://uart, а для
 * UDP порта nmea://udp.
 *
 * Если параметры подключения не указаны, для UART порта запускается процесс
 * автоматического поиска подключенных датчиков на всех доступных UART портах.
 * Для UDP осуществляется приём данных на всех IP адресах и порту номер 10000.
 *
 * Для создания класса предназначена функция #hyscan_nmea_driver_new.
 *
 * Описание параметров подключения можно получить с помощью функции
 * #hyscan_nmea_driver_get_connect_schema.
 */

#include "hyscan-nmea-driver.h"
#include "hyscan-nmea-uart.h"
#include "hyscan-nmea-udp.h"
#include "hyscan-nmea-drv.h"

#include <hyscan-param-controller.h>
#include <hyscan-device-driver.h>
#include <hyscan-sensor-driver.h>
#include <hyscan-buffer.h>

#include <glib/gi18n-lib.h>
#include <string.h>

#define PARAM_DEVICE_ID            "/dev-id"
#define PARAM_TIMEOUT_WARNING      "/timeout/warning"
#define PARAM_TIMEOUT_ERROR        "/timeout/error"
#define PARAM_UART_PORT            "/uart/port"
#define PARAM_UART_MODE            "/uart/mode"
#define PARAM_UDP_ADDRESS          "/udp/address"
#define PARAM_UDP_PORT             "/udp/port"

#define DEFAULT_WARNING_TIMEOUT    5.0
#define DEFAULT_ERROR_TIMEOUT      30.0
#define DEFAULT_UDP_PORT           10000

#define NMEA_INFO_NAME(...)        hyscan_param_name_constructor (key_id, \
                                     (guint)sizeof (key_id), "info", __VA_ARGS__)

#define NMEA_STATE_NAME(...)       hyscan_param_name_constructor (key_id, \
                                     (guint)sizeof (key_id), "state", __VA_ARGS__)

enum
{
  PROP_O,
  PROP_URI,
  PROP_PARAMS
};

/* Параметры работы устройства. */
typedef struct
{
  gchar                  *dev_id;              /* Идентификатор датчика. */
  gint64                  uart_port;           /* Идентификатор UART порта. */
  gint64                  uart_mode;           /* Режим работы UART порта. */
  gint64                  udp_address;         /* Идентификатор IP адреса UDP порта. */
  gint64                  udp_port;            /* Номер UDP порта. */
  gdouble                 warning_timeout;     /* Таймаут приёма данных - предупреждение. */
  gdouble                 error_timeout;       /* Таймаут приёма данных - перезапуск порта. */
} HyScanNmeaDriverParams;

struct _HyScanNmeaDriverPrivate
{
  gchar                  *uri;                 /* Путь к датчику. */
  HyScanNmeaDriverParams  params;              /* Параметры драйвера. */

  HyScanDataSchema       *schema;              /* Схема датчика. */
  gboolean                enable;              /* Признак активности датчика. */

  gboolean                shutdown;            /* Признак завершения работы. */
  GThread                *starter;             /* Поток подключения к NMEA датчикам. */
  GThread                *scanner;             /* Поток поиска NMEA UART датчиков. */

  GObject                *transport;           /* Класс приёма данных от датчика. */
  gboolean                io_error;            /* Признак ошибки ввода вывода. */
  HyScanBuffer           *buffer;              /* Буфер данных. */

  gint                    status;              /* Статус датчика. */
  gint                    prev_status;         /* Предыдущий статус датчика. */
  gchar                  *status_name;         /* Название параметра статуса. */

  GTimer                 *data_timer;          /* Таймер приёма данных. */
};

static void      hyscan_nmea_driver_param_interface_init   (HyScanParamInterface    *iface);
static void      hyscan_nmea_driver_device_interface_init  (HyScanDeviceInterface   *iface);
static void      hyscan_nmea_driver_sensor_interface_init  (HyScanSensorInterface   *iface);

static void      hyscan_nmea_driver_set_property           (GObject                 *object,
                                                            guint                    prop_id,
                                                            const GValue            *value,
                                                            GParamSpec              *pspec);
static void      hyscan_nmea_driver_object_constructed     (GObject                 *object);
static void      hyscan_nmea_driver_object_finalize        (GObject                 *object);

static void      hyscan_nmea_driver_parse_connect_params   (HyScanParamList         *list,
                                                            HyScanNmeaDriverParams  *params);

static HyScanDataSchema *
                 hyscan_nmea_driver_create_schema          (const gchar             *dev_id);

static void      hyscan_nmea_driver_disconnect             (HyScanNmeaDriverPrivate *priv);

static gpointer  hyscan_nmea_driver_starter                (gpointer                 user_data);

static gpointer  hyscan_nmea_driver_scanner                (gpointer                 user_data);

static void      hyscan_nmea_driver_check_data             (HyScanNmeaDriver        *driver);

static void      hyscan_nmea_driver_io_error               (HyScanNmeaReceiver      *receiver,
                                                            HyScanNmeaDriver        *driver);

static void      hyscan_nmea_driver_tester                 (HyScanNmeaReceiver      *receiver,
                                                            gint64                   time,
                                                            const gchar             *data,
                                                            guint                    size,
                                                            HyScanNmeaDriver        *driver);

static void      hyscan_nmea_driver_emmiter                (HyScanNmeaReceiver      *receiver,
                                                            gint64                   time,
                                                            const gchar             *data,
                                                            guint                    size,
                                                            HyScanNmeaDriver        *driver);

G_DEFINE_TYPE_WITH_CODE (HyScanNmeaDriver, hyscan_nmea_driver, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanNmeaDriver)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM,  hyscan_nmea_driver_param_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DEVICE, hyscan_nmea_driver_device_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR, hyscan_nmea_driver_sensor_interface_init))

static void
hyscan_nmea_driver_class_init (HyScanNmeaDriverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_nmea_driver_set_property;

  object_class->constructed = hyscan_nmea_driver_object_constructed;
  object_class->finalize = hyscan_nmea_driver_object_finalize;

  g_object_class_install_property (object_class, PROP_URI,
    g_param_spec_string ("uri", "URI", "Sensor uri", NULL,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PARAMS,
    g_param_spec_object ("params", "Params", "Connection parameters", HYSCAN_TYPE_PARAM_LIST,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_nmea_driver_init (HyScanNmeaDriver *driver)
{
  driver->priv = hyscan_nmea_driver_get_instance_private (driver);
  HyScanNmeaDriverParams *params = &driver->priv->params;

  /* По умолчанию устанавливаем автоматический выбор скорости
   * UART порта и номер UDP порта в 10000. */
  params->uart_mode = HYSCAN_NMEA_UART_MODE_AUTO;
  params->udp_port = DEFAULT_UDP_PORT;

  /* Таймауты по умолчанию. */
  params->warning_timeout = DEFAULT_WARNING_TIMEOUT;
  params->error_timeout = DEFAULT_ERROR_TIMEOUT;
}

static void
hyscan_nmea_driver_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (object);
  HyScanNmeaDriverPrivate *priv = driver->priv;

  switch (prop_id)
    {
    case PROP_URI:
      priv->uri = g_value_dup_string (value);
      break;

    case PROP_PARAMS:
      hyscan_nmea_driver_parse_connect_params (g_value_get_object (value), &priv->params);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_nmea_driver_object_constructed (GObject *object)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (object);
  HyScanNmeaDriverPrivate *priv = driver->priv;

  if (priv->uri == NULL)
    return;

  /* Идентификатор датчика. */
  if (priv->params.dev_id == NULL)
    priv->params.dev_id = g_strdup ("nmea");

  /* Начальный статус. */
  priv->status = HYSCAN_DEVICE_STATUS_ERROR;
  priv->prev_status = HYSCAN_DEVICE_STATUS_ERROR;

  /* Таймер данных. */
  priv->data_timer = g_timer_new ();

  /* Буфер данных. */
  priv->buffer = hyscan_buffer_new ();

  /* Автоматический выбор UART порта и режима работы. */
  if ((g_ascii_strcasecmp (priv->uri, HYSCAN_NMEA_DRIVER_UART_URI) == 0) &&
      (priv->params.uart_port == 0))
    {
      priv->scanner = g_thread_new ("uart-scanner", hyscan_nmea_driver_scanner, driver);
    }

  /* Конкретный UART или UDP порт. */
  else
    {
      priv->starter = g_thread_new ("uart-starter", hyscan_nmea_driver_starter, driver);
    }

  /* Название параметра статуса. */
  priv->status_name = g_strdup_printf ("/state/%s/status", priv->params.dev_id);

  /* Схема датчика. */
  priv->schema = hyscan_nmea_driver_create_schema (priv->params.dev_id);
}

static void
hyscan_nmea_driver_object_finalize (GObject *object)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (object);
  HyScanNmeaDriverPrivate *priv = driver->priv;

  hyscan_nmea_driver_disconnect (priv);
  g_clear_pointer (&priv->data_timer, g_timer_destroy);
  g_clear_object (&priv->buffer);
  g_clear_object (&priv->schema);
  g_free (priv->status_name);
  g_free (priv->params.dev_id);
  g_free (priv->uri);

  G_OBJECT_CLASS (hyscan_nmea_driver_parent_class)->finalize (object);
}

/* Функция разбирает параметры подключения. */
static void
hyscan_nmea_driver_parse_connect_params (HyScanParamList        *list,
                                         HyScanNmeaDriverParams *params)
{
  HyScanParamController *controller;
  HyScanDataSchema *schema;
  GString *dev_id;

  if ((list == NULL) || (hyscan_param_list_params (list) == NULL))
    return;

  dev_id = g_string_new (NULL);
  controller = hyscan_param_controller_new (NULL);

  schema = hyscan_nmea_driver_get_connect_schema (NULL, TRUE);
  hyscan_param_controller_set_schema (controller, schema);

  hyscan_param_controller_add_string (controller, PARAM_DEVICE_ID, dev_id);
  hyscan_param_controller_add_double (controller, PARAM_TIMEOUT_WARNING, &params->warning_timeout);
  hyscan_param_controller_add_double (controller, PARAM_TIMEOUT_ERROR, &params->error_timeout);
  hyscan_param_controller_add_enum   (controller, PARAM_UART_PORT, &params->uart_port);
  hyscan_param_controller_add_enum   (controller, PARAM_UART_MODE, &params->uart_mode);
  hyscan_param_controller_add_enum   (controller, PARAM_UDP_ADDRESS, &params->udp_address);
  hyscan_param_controller_add_enum   (controller, PARAM_UDP_PORT, &params->udp_port);

  if (!hyscan_param_set (HYSCAN_PARAM (controller), list))
    g_warning ("HyScanNmeaDriver: error in connect params");

  params->dev_id = g_string_free (dev_id, (dev_id->len == 0));

  g_object_unref (controller);
  g_object_unref (schema);
}

/* Функция создаёт схему датчика. */
static HyScanDataSchema *
hyscan_nmea_driver_create_schema (const gchar *dev_id)
{
  HyScanDataSchemaBuilder *builder;
  HyScanDeviceSchema *device;
  HyScanSensorSchema *sensor;
  HyScanDataSchema *schema;
  gchar key_id[128];

  device = hyscan_device_schema_new (HYSCAN_DEVICE_SCHEMA_VERSION);
  sensor = hyscan_sensor_schema_new (device);
  builder = HYSCAN_DATA_SCHEMA_BUILDER (device);

  /* Описание датчика. */
  hyscan_sensor_schema_add_sensor (sensor, dev_id, dev_id, _("NMEA sensor"));

  /* Информация о датчике. */

  /* Название устройства. */
  NMEA_INFO_NAME (dev_id, NULL);
  hyscan_data_schema_builder_node_set_name     (builder, key_id, "Nmea", dev_id);

  /* Название датчика. */
  NMEA_INFO_NAME (dev_id, "name", NULL);
  hyscan_data_schema_builder_key_string_create (builder, key_id,
                                                _("Name"), _("Sensor name"),
                                                dev_id);
  hyscan_data_schema_builder_key_set_access    (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READ);

  /* Версия драйвера. */
  NMEA_INFO_NAME (dev_id, "drv", NULL);
  hyscan_data_schema_builder_key_string_create (builder, key_id,
                                                _("Driver"), _("Driver"),
                                                "Nmea");
  hyscan_data_schema_builder_key_set_access    (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READ);

  /* Версия драйвера. */
  NMEA_INFO_NAME (dev_id, "drv-version", NULL);
  hyscan_data_schema_builder_key_string_create (builder, key_id,
                                                _("Driver version"), _("Driver version"),
                                                HYSCAN_NMEA_DRIVER_VERSION);
  hyscan_data_schema_builder_key_set_access    (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READ);

  /* Версия драйвера. */
  NMEA_INFO_NAME (dev_id, "drv-build-id", NULL);
  hyscan_data_schema_builder_key_string_create (builder, key_id,
                                                _("Driver build id"), _("Driver build id"),
                                                HYSCAN_NMEA_DRIVER_BUILD_ID);
  hyscan_data_schema_builder_key_set_access    (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READ);

  /* Статус работы датчика. */
  NMEA_STATE_NAME (dev_id, "status", NULL);
  hyscan_data_schema_builder_key_enum_create (builder, key_id, "Status", NULL,
                                              HYSCAN_DEVICE_STATUS_ENUM, HYSCAN_DEVICE_STATUS_ERROR);
  hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READ);

  schema = hyscan_data_schema_builder_get_schema (builder);

  g_object_unref (sensor);
  g_object_unref (builder);

  return schema;
}

/* Функция производит отключение от устройства. */
static void
hyscan_nmea_driver_disconnect (HyScanNmeaDriverPrivate *priv)
{
  g_atomic_int_set (&priv->shutdown, TRUE);
  g_clear_pointer (&priv->starter, g_thread_join);
  g_clear_pointer (&priv->scanner, g_thread_join);

  g_clear_object (&priv->transport);
}

/* Поток подключения к NMEA датчикам. */
static gpointer
hyscan_nmea_driver_starter (gpointer user_data)
{
  HyScanNmeaDriver *driver = user_data;
  HyScanNmeaDriverPrivate *priv = driver->priv;
  HyScanNmeaDriverParams *params = &priv->params;

  while (!g_atomic_int_get (&priv->shutdown))
    {
      /* Подключение установлено - проверяем приём данных. */
      if (g_atomic_pointer_get (&priv->transport) != NULL)
        {
          hyscan_nmea_driver_check_data (driver);
        }

      /* Определённый UART порт. */
      else if (g_ascii_strcasecmp (priv->uri, HYSCAN_NMEA_DRIVER_UART_URI) == 0)
        {
          HyScanNmeaUART *uart;
          gchar *uart_path = NULL;
          GList *devices, *device;

          /* Ищем путь к устройству по идентификатору UART порта. */
          device = devices = hyscan_nmea_uart_list_devices ();
          while (device != NULL)
            {
              HyScanNmeaUARTDevice *info = device->data;
              guint port_id = g_str_hash (info->path);

              if (port_id == params->uart_port)
                {
                  uart_path = g_strdup (info->path);
                  break;
                }

              device = g_list_next (device);
            }
          g_list_free_full (devices, (GDestroyNotify)hyscan_nmea_uart_device_free);

          /* Открываем порт. */
          if (uart_path != NULL)
            {
              uart = hyscan_nmea_uart_new ();

              if (!hyscan_nmea_uart_set_device (uart, uart_path, params->uart_mode))
                {
                  g_clear_object (&uart);
                }
              else
                {
                  priv->transport = G_OBJECT (uart);

                  g_signal_connect (priv->transport, "nmea-data",
                                    G_CALLBACK (hyscan_nmea_driver_emmiter), driver);
                  g_signal_connect (priv->transport, "nmea-io-error",
                                    G_CALLBACK (hyscan_nmea_driver_io_error), driver);
                }

              g_free (uart_path);
            }
        }

      /* Определённый UDP порт. */
      else if (g_ascii_strcasecmp (priv->uri, HYSCAN_NMEA_DRIVER_UDP_URI) == 0)
        {
          HyScanNmeaUDP *udp;
          gchar *address = NULL;

          /* Выбраны все адреса. */
          if (params->udp_address == 0)
            {
              address = g_strdup ("any");
            }

          /* Loopback адрес. */
          else if (params->udp_address == 1)
            {
              address = g_strdup ("loopback");
            }

          /* Ищем выбранный адрес по его идентификатору. */
          else
            {
              gchar **addresses = hyscan_nmea_udp_list_addresses ();
              guint i;

              for (i = 0; (addresses != NULL) && (addresses[i] != NULL); i++)
                {
                  guint address_id = g_str_hash (addresses[i]);

                  if (address_id == params->udp_address)
                    address = g_strdup (addresses [i]);
                }

              g_strfreev (addresses);
            }

          if (address != NULL)
            {
              udp = hyscan_nmea_udp_new ();

              if (!hyscan_nmea_udp_set_address (udp, address, params->udp_port))
                {
                  g_clear_object (&udp);
                }
              else
                {
                  priv->transport = G_OBJECT (udp);

                  g_signal_connect (priv->transport, "nmea-data",
                                    G_CALLBACK (hyscan_nmea_driver_emmiter), driver);
                  g_signal_connect (priv->transport, "nmea-io-error",
                                    G_CALLBACK (hyscan_nmea_driver_io_error), driver);
                }

              g_free (address);
            }
        }

      g_usleep (100000);
    }

  return NULL;
}

/* Поток автоматического поиска подключенных NMEA UART датчиков. */
static gpointer
hyscan_nmea_driver_scanner (gpointer user_data)
{
  HyScanNmeaDriver *driver = user_data;
  HyScanNmeaDriverPrivate *priv = driver->priv;

  GTimer *timer = g_timer_new ();
  GList *uarts = NULL;

  while (!g_atomic_int_get (&priv->shutdown))
    {
      /* Порт с данным найден. */
      if (g_atomic_pointer_get (&priv->transport) != NULL)
        {
          /* Останавливаем поиск. */
          if (uarts != NULL)
            {
              g_list_free_full (uarts, g_object_unref);
              uarts = NULL;
            }

          /* Проверка приёма данных. */
          hyscan_nmea_driver_check_data (driver);
        }

      /* Запускаем поиск данных на всех портах и смотрим где появятся данные. */
      else if (uarts == NULL)
        {
          GList *devices, *device;

          device = devices = hyscan_nmea_uart_list_devices ();
          while (device != NULL)
            {
              HyScanNmeaUART *uart = hyscan_nmea_uart_new ();
              HyScanNmeaUARTDevice *info = device->data;

              if (hyscan_nmea_uart_set_device (uart, info->path, HYSCAN_NMEA_UART_MODE_AUTO))
                uarts = g_list_prepend (uarts, uart);
              else
                g_clear_object (&uart);

              if (uart != NULL)
                g_signal_connect (uart, "nmea-data", G_CALLBACK (hyscan_nmea_driver_tester), driver);

              device = g_list_next (device);
            }
          g_list_free_full (devices, (GDestroyNotify)hyscan_nmea_uart_device_free);

          g_timer_start (timer);
        }

      /* За 25 секунд дважды изменяются все возможные скорости работы порта,
       * поэтому останавливаем поиск. Позже он будет запущен вновь с
       * обновлённым списоком портов. */
      else if (g_timer_elapsed (timer, NULL) > 25.0)
        {
          if (uarts != NULL)
            {
              g_list_free_full (uarts, g_object_unref);
              uarts = NULL;
            }
        }

      g_usleep (100000);
    }

  g_list_free_full (uarts, g_object_unref);
  g_timer_destroy (timer);

  return NULL;
}

/* Функция проверяет приём данных и перезапускает порт при необходимости. */
static void
hyscan_nmea_driver_check_data (HyScanNmeaDriver *driver)
{
  HyScanNmeaDriverPrivate *priv = driver->priv;
  HyScanNmeaDriverParams *params = &priv->params;

  gdouble data_timeout = g_timer_elapsed (priv->data_timer, NULL);
  gint cur_status = g_atomic_int_get (&priv->status);
  gboolean io_error = FALSE;

  /* Ошибка ввода/вывода - перезапускаем порт. */
  if (g_atomic_int_get (&priv->io_error))
    {
      g_object_unref (priv->transport);
      g_atomic_pointer_set (&priv->transport, NULL);

      g_atomic_int_set (&priv->status, HYSCAN_DEVICE_STATUS_ERROR);
      g_atomic_int_set (&priv->io_error, FALSE);

      cur_status = HYSCAN_DEVICE_STATUS_ERROR;
      io_error = TRUE;
    }

  /* Данных нет длительное время. */
  else if (data_timeout > params->error_timeout)
    {
      g_atomic_int_set (&priv->status, HYSCAN_DEVICE_STATUS_ERROR);
      cur_status = HYSCAN_DEVICE_STATUS_ERROR;
    }

  /* Посылаем предупреждение. */
  else if (data_timeout > params->warning_timeout)
    {
      gboolean changed;
      changed = g_atomic_int_compare_and_exchange (&priv->status,
                                                   HYSCAN_DEVICE_STATUS_OK,
                                                   HYSCAN_DEVICE_STATUS_WARNING);
      if (changed)
        cur_status = HYSCAN_DEVICE_STATUS_WARNING;
    }

  /* Изменился статус. */
  if (g_atomic_int_get (&priv->prev_status) != cur_status)
    {
      gchar message[256];

      if (cur_status == HYSCAN_DEVICE_STATUS_OK)
        {
          g_snprintf (message, sizeof (message),
                      "The sensor is fully operational.");
        }
      else if (cur_status == HYSCAN_DEVICE_STATUS_WARNING)
        {
          g_snprintf (message, sizeof (message),
                      "Temporary error while receiving data.");
        }
      else
        {
          g_snprintf (message, sizeof (message),
                      "An error occurred while receiving data%s",
                      io_error ? ", port disconnected." : ".");

        }

      hyscan_device_driver_send_state (driver, params->dev_id);
      hyscan_device_driver_send_log (driver, params->dev_id,
                                     g_get_monotonic_time (),
                                     HYSCAN_LOG_LEVEL_INFO,
                                     message);

      g_atomic_int_set (&priv->prev_status, cur_status);
    }
}

/* Функция регистрирует сигнал ошибки чтения данных от устройства. */
static void
hyscan_nmea_driver_io_error (HyScanNmeaReceiver *receiver,
                             HyScanNmeaDriver   *driver)
{
  g_atomic_int_set (&driver->priv->io_error, TRUE);
}

/* Функция проверяет приём данных от UART датчика. */
static void
hyscan_nmea_driver_tester (HyScanNmeaReceiver *receiver,
                           gint64              time,
                           const gchar        *data,
                           guint               size,
                           HyScanNmeaDriver   *driver)
{
  HyScanNmeaDriverPrivate *priv = driver->priv;

  if (g_atomic_pointer_compare_and_exchange (&priv->transport, NULL, receiver))
    {
      g_signal_handlers_disconnect_by_func (receiver, hyscan_nmea_driver_tester, driver);

      g_signal_connect (receiver, "nmea-data",
                        G_CALLBACK (hyscan_nmea_driver_emmiter), driver);
      g_signal_connect (receiver, "nmea-io-error",
                        G_CALLBACK (hyscan_nmea_driver_io_error), driver);

      g_object_ref (receiver);
    }
}

/* Функция отправки данных. */
static void
hyscan_nmea_driver_emmiter (HyScanNmeaReceiver *receiver,
                            gint64              time,
                            const gchar        *data,
                            guint               size,
                            HyScanNmeaDriver   *driver)
{
  HyScanNmeaDriverPrivate *priv = driver->priv;

  /* Сбрасываем таймер таймаута данных. */
  g_timer_start (priv->data_timer);

  /* Сигнализируем о приёме данных. */
  g_atomic_int_set (&priv->status, HYSCAN_DEVICE_STATUS_OK);

  /* Приём данных отключен. */
  if (!g_atomic_int_get (&priv->enable))
    return;

  /* Отправка всех NMEA данных. */
  hyscan_buffer_wrap (priv->buffer, HYSCAN_DATA_STRING, (gpointer)data, size);
  hyscan_sensor_driver_send_data (driver, priv->params.dev_id,
                                  HYSCAN_SOURCE_NMEA, time, priv->buffer);
}

static HyScanDataSchema *
hyscan_nmea_driver_param_schema (HyScanParam *param)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (param);
  HyScanNmeaDriverPrivate *priv = driver->priv;

  return g_object_ref (priv->schema);
}

static gboolean
hyscan_nmea_driver_param_get (HyScanParam      *param,
                              HyScanParamList  *list)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (param);
  HyScanNmeaDriverPrivate *priv = driver->priv;
  const gchar * const *params;

  params = hyscan_param_list_params (list);
  if ((params == NULL) || (g_strcmp0 (params[0], priv->status_name) != 0))
    return FALSE;

  hyscan_param_list_set_enum (list, priv->status_name, g_atomic_int_get (&priv->status));

  return TRUE;
}

static gboolean
hyscan_nmea_driver_sensor_set_enable (HyScanSensor *sensor,
                                      const gchar  *name,
                                      gboolean      enable)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (sensor);
  HyScanNmeaDriverPrivate *priv = driver->priv;

  if (g_strcmp0 (priv->params.dev_id, name) != 0)
    return FALSE;

  g_atomic_int_set (&priv->enable, enable);

  return TRUE;
}

static gboolean
hyscan_nmea_driver_device_disconnect (HyScanDevice *sensor)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (sensor);

  hyscan_nmea_driver_disconnect (driver->priv);

  return TRUE;
}

/**
 * hyscan_nmea_driver_new:
 * @uri: путь к датчику
 * @params: параметры подключения датчика
 *
 * Функция создаёт новый объект #HyScanNmeaDriver.
 *
 * Returns: (nullable): #HyScanNmeaDriver или NULL. Для удаления #g_object_unref.
 */
HyScanNmeaDriver *
hyscan_nmea_driver_new (const gchar     *uri,
                        HyScanParamList *params)
{
  HyScanNmeaDriver *driver;

  driver = g_object_new (HYSCAN_TYPE_NMEA_DRIVER,
                         "uri", uri,
                         "params", params,
                         NULL);

  if (driver->priv->schema == NULL)
    g_clear_object (&driver);

  return driver;
}

/**
 * hyscan_nmea_driver_get_uart_schema:
 * @uri: путь к датчику
 * @connect: признак
 *
 * Функция возвращает схему параметров подключения к NMEA датчику.
 *
 * Returns: #HyScanDataSchema или %NULL в случае ошибочного uri.
 * Для удаления #g_object_unref.
 */
HyScanDataSchema *
hyscan_nmea_driver_get_connect_schema (const gchar *uri,
                                       gboolean     full)
{
  HyScanDataSchemaBuilder *builder;
  HyScanDataSchema *schema;

  if ((uri == NULL) && !full)
    return NULL;

  builder = hyscan_data_schema_builder_new ("params");

  /* Идентификатор устройства. */
  hyscan_data_schema_builder_key_string_create (builder, PARAM_DEVICE_ID,
                                                _("Device id"), NULL, "nmea");

  /* Таймауты приёма данных. */
  hyscan_data_schema_builder_key_double_create (builder, PARAM_TIMEOUT_WARNING,
                                                _("Timeout before warning"), NULL,
                                                DEFAULT_WARNING_TIMEOUT);
  hyscan_data_schema_builder_key_double_range  (builder, PARAM_TIMEOUT_WARNING,
                                                0.0, 30.0, 1.0);

  hyscan_data_schema_builder_key_double_create (builder, PARAM_TIMEOUT_ERROR,
                                                _("Timeout before error"), NULL,
                                                DEFAULT_ERROR_TIMEOUT);
  hyscan_data_schema_builder_key_double_range  (builder, PARAM_TIMEOUT_ERROR,
                                                30.0, 60.0, 1.0);

  /* Параметры UART порта. */
  if (full || (g_ascii_strcasecmp (uri, HYSCAN_NMEA_DRIVER_UART_URI) == 0))
    {
      GList *devices, *device;

      /* Список UART портов. */
      hyscan_data_schema_builder_enum_create (builder, "uart-port");

      hyscan_data_schema_builder_enum_value_create (builder, "uart-port",
                                                    0, "auto",
                                                    _("Auto select"), NULL);

      device = devices = hyscan_nmea_uart_list_devices ();
      while (device != NULL)
        {
          HyScanNmeaUARTDevice *info = device->data;
          guint port_id = g_str_hash (info->path);

          hyscan_data_schema_builder_enum_value_create (builder, "uart-port",
                                                        port_id, info->name,
                                                        info->name, NULL);

          device = g_list_next (device);
        }
      g_list_free_full (devices, (GDestroyNotify)hyscan_nmea_uart_device_free);

      hyscan_data_schema_builder_key_enum_create (builder, PARAM_UART_PORT,
                                                  _("Port"), NULL,
                                                  "uart-port", 0);

      /* Режимы работы UART порта. */
      hyscan_data_schema_builder_enum_create (builder, "uart-mode");

      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode",
                                                    HYSCAN_NMEA_UART_MODE_AUTO, "auto",
                                                    _("Auto select"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode",
                                                    HYSCAN_NMEA_UART_MODE_4800_8N1, "4800-8N1",
                                                    _("4800 8N1"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode",
                                                    HYSCAN_NMEA_UART_MODE_9600_8N1, "9600-8N1",
                                                    _("9600 8N1"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode",
                                                    HYSCAN_NMEA_UART_MODE_19200_8N1, "19200-8N1",
                                                    _("19200 8N1"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode",
                                                    HYSCAN_NMEA_UART_MODE_38400_8N1, "38400-8N1",
                                                    _("38400 8N1"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode",
                                                    HYSCAN_NMEA_UART_MODE_57600_8N1, "57600-8N1",
                                                    _("57600 8N1"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode",
                                                    HYSCAN_NMEA_UART_MODE_115200_8N1, "115200-8N1",
                                                    _("115200 8N1"), NULL);

      hyscan_data_schema_builder_key_enum_create (builder, PARAM_UART_MODE,
                                                  _("Mode"), NULL,
                                                  "uart-mode", HYSCAN_NMEA_UART_MODE_AUTO);
    }

  /* Параметры UDP порта. */
  if (full || (g_ascii_strcasecmp (uri, HYSCAN_NMEA_DRIVER_UDP_URI) == 0))
    {
      gchar **addresses;
      guint i;

      /* Список IP адресов. */
      hyscan_data_schema_builder_enum_create (builder, "udp-address");

      hyscan_data_schema_builder_enum_value_create (builder, "udp-address",
                                                    0, "all",
                                                    _("All addresses"), NULL);

      addresses = hyscan_nmea_udp_list_addresses ();
      for (i = 0; (addresses != NULL) && (addresses[i] != NULL); i++)
        {
          guint address_id = g_str_hash (addresses[i]);

          hyscan_data_schema_builder_enum_value_create (builder, "udp-address",
                                                        address_id, addresses[i],
                                                        addresses[i], NULL);
        }
      g_strfreev (addresses);

      hyscan_data_schema_builder_key_enum_create (builder, PARAM_UDP_ADDRESS,
                                                  _("Address"), NULL,
                                                  "udp-address", 0);

      /* UDP/IP порт. */
      hyscan_data_schema_builder_key_integer_create (builder, PARAM_UDP_PORT,
                                                     _("UDP port"), NULL, DEFAULT_UDP_PORT);
      hyscan_data_schema_builder_key_integer_range  (builder, PARAM_UDP_PORT,
                                                     1024, 65535, 1);
    }

  schema = hyscan_data_schema_builder_get_schema (builder);

  g_object_unref (builder);

  return schema;
}

/**
 * hyscan_nmea_driver_check_connect:
 * @uri: путь к датчика
 * @params: параметры драйвера
 *
 * Функция проверяет возможность подключения к датчику для указанного пути
 * и параметров драйвера.
 *
 * Returns: %TRUE если подключение возможно, иначе %FALSE.
 */
gboolean
hyscan_nmea_driver_check_connect (const gchar     *uri,
                                  HyScanParamList *params)
{
  HyScanDataSchema *schema;
  const gchar * const *names;
  gboolean status = FALSE;
  guint i;

  schema = hyscan_nmea_driver_get_connect_schema (uri, FALSE);
  if (schema == NULL)
    goto exit;

  status = TRUE;

  if (params == NULL)
    goto exit;

  names = hyscan_param_list_params (params);
  if (names == NULL)
    goto exit;

  for (i = 0; names[i] != NULL; i++)
    {
      GVariant *value = hyscan_param_list_get (params, names[i]);
      if (!hyscan_data_schema_key_check (schema, names[i], value))
        status = FALSE;
      g_variant_unref (value);
    }

exit:
  g_clear_object (&schema);
  return status;
}

static void
hyscan_nmea_driver_param_interface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_nmea_driver_param_schema;
  iface->set = NULL;
  iface->get = hyscan_nmea_driver_param_get;
}

static void
hyscan_nmea_driver_device_interface_init (HyScanDeviceInterface *iface)
{
  iface->set_sound_velocity = NULL;
  iface->disconnect = hyscan_nmea_driver_device_disconnect;
}

static void
hyscan_nmea_driver_sensor_interface_init (HyScanSensorInterface *iface)
{
  iface->set_enable = hyscan_nmea_driver_sensor_set_enable;
}
