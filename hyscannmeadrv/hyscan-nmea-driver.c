/* hyscan-nmea-driver.c
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
 * SECTION: hyscan-nmea-driver
 * @Short_description: класс драйвера NMEA датчика
 * @Title: HyScanNmeaDriver
 *
 * Класс реализует драйвер NMEA датчика обеспечивая приём данных через UART
 * порт или UDP порт. Класс реализует интерфейсы #HyScanSensor и #HyScanParam.
 * Выбор типа подключения осуществляется через путь к датчику - URI.
 * Для подключения через UART порт должен быть указан путь NMEA://UART, а для
 * UDP порта NMEA://UDP.
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

#include <hyscan-sensor-schema.h>
#include <hyscan-buffer.h>

#include <glib/gi18n-lib.h>
#include <string.h>

#define TIMEOUT_WARNING_PARAM  "/timeout/warning"
#define TIMEOUT_ERROR_PARAM    "/timeout/error"
#define UART_PORT_PARAM        "/uart/port"
#define UART_MODE_PARAM        "/uart/mode"
#define UDP_ADDRESS_PARAM      "/udp/address"
#define UDP_PORT_PARAM         "/udp/port"

enum
{
  PROP_O,
  PROP_URI,
  PROP_PARAMS
};

enum
{
  NMEA_DRIVER_STATUS_OK,
  NMEA_DRIVER_STATUS_WARNING,
  NMEA_DRIVER_STATUS_ERROR
};

struct _HyScanNmeaDriverPrivate
{
  gchar               *uri;                    /* Путь к датчику. */

  HyScanDataSchema    *connect_schema;         /* Схема параметров подключения. */
  HyScanDataSchema    *sensor_schema;          /* Схема датчика. */
  gboolean             enable;                 /* Признак активности датчика. */

  gboolean             shutdown;               /* Признак завершения работы. */
  GThread             *starter;                /* Поток подключения к NMEA датчикам. */
  GThread             *scanner;                /* Поток поиска NMEA UART датчиков. */

  GObject             *transport;              /* Класс приёма данных от датчика. */
  gboolean             io_error;               /* Признак ошибки ввода вывода. */
  HyScanBuffer        *buffer;                 /* Буфер данных. */

  gchar               *name;                   /* Название датчика. */

  guint                uart_port;              /* Идентификатор UART порта. */
  HyScanNmeaUARTMode   uart_mode;              /* Режим работы UART порта. */
  guint                udp_address;            /* Идентификатор IP адреса UDP порта. */
  guint16              udp_port;               /* Номер UDP порта. */

  gint                 status;                 /* Статус датчика. */
  gint                 prev_status;            /* Предыдущий статус датчика. */

  GTimer              *data_timer;             /* Таймер приёма данных. */
  gdouble              warning_timeout;        /* Таймаут приёма данных - предупреждение. */
  gdouble              error_timeout;          /* Таймаут приёма данных - перезапуск порта. */

  gchar               *state_prefix   ;        /* Префикс параметров /state. */
};

static void        hyscan_nmea_driver_param_interface_init     (HyScanParamInterface  *iface);
static void        hyscan_nmea_driver_sensor_interface_init    (HyScanSensorInterface *iface);

static void        hyscan_nmea_driver_set_property             (GObject                *object,
                                                                guint                   prop_id,
                                                                const GValue           *value,
                                                                GParamSpec             *pspec);
static void        hyscan_nmea_driver_object_constructed       (GObject                *object);
static void        hyscan_nmea_driver_object_finalize          (GObject                *object);

static HyScanDataSchema *
                   hyscan_nmea_driver_create_schema            (const gchar            *name,
                                                                const gchar            *dev_id);

static gpointer    hyscan_nmea_driver_starter                  (gpointer                user_data);

static gpointer    hyscan_nmea_driver_scanner                  (gpointer                user_data);

static void        hyscan_nmea_driver_check_data               (HyScanNmeaDriver       *driver);

static void        hyscan_nmea_driver_io_error                 (HyScanNmeaReceiver     *receiver,
                                                                HyScanNmeaDriver       *driver);

static void        hyscan_nmea_driver_tester                   (HyScanNmeaReceiver     *receiver,
                                                                gint64                  time,
                                                                const gchar            *data,
                                                                guint                   size,
                                                                HyScanNmeaDriver       *driver);

static void        hyscan_nmea_driver_emmiter                  (HyScanNmeaReceiver     *receiver,
                                                                gint64                  time,
                                                                const gchar            *data,
                                                                guint                   size,
                                                                HyScanNmeaDriver       *driver);

static guint       driver_id = 0;

G_DEFINE_TYPE_WITH_CODE (HyScanNmeaDriver, hyscan_nmea_driver, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanNmeaDriver)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM,  hyscan_nmea_driver_param_interface_init)
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

  /* По умолчанию устанавливаем автоматический выбор скорости
   * UART порта и номер UDP порта в 10000. */
  driver->priv->uart_mode = HYSCAN_NMEA_UART_MODE_AUTO;
  driver->priv->udp_port = 10000;

  /* Таймауты по умолчанию. */
  driver->priv->warning_timeout = 5.0;
  driver->priv->error_timeout = 30.0;

  g_atomic_int_add (&driver_id, 1);
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
      {
        priv->uri = g_ascii_strup (g_value_get_string (value), -1);
        priv->connect_schema = hyscan_nmea_driver_get_connect_schema (priv->uri);
      }
      break;

    case PROP_PARAMS:
      {
        HyScanParamList *list = g_value_get_object (value);
        const gchar * const *params;
        gboolean bad_value = FALSE;
        guint i;

        /* Должна быть известна схема подключения. */
        if ((list == NULL) || (priv->connect_schema == NULL))
          break;

        /* Список параметров. */
        params = hyscan_param_list_params (list);
        if (params == NULL)
          break;

        /* Проверяем значения параметров по схеме подключения. */
        for (i = 0; params[i] != NULL; i++)
          {
            GVariant *value = hyscan_param_list_get (list, params[i]);

            if (!hyscan_data_schema_has_key (priv->connect_schema, params[i]))
              {
                g_warning ("HyScanNmeaDriver: unsupported parameter '%s'", params[i]);
                bad_value = TRUE;
              }

            if (!hyscan_data_schema_key_check (priv->connect_schema, params[i], value))
              {
                g_warning ("HyScanNmeaDriver: bad value for parameter '%s'", params[i]);
                bad_value = TRUE;
              }

            g_clear_pointer (&value, g_variant_unref);
          }

        /* Ошибка в параметрах. */
        if (bad_value)
          break;

        /* Таймауты. */
        if (hyscan_param_list_contains (list, TIMEOUT_WARNING_PARAM))
          priv->warning_timeout = hyscan_param_list_get_double (list, TIMEOUT_WARNING_PARAM);
        if (hyscan_param_list_contains (list, TIMEOUT_ERROR_PARAM))
          priv->error_timeout = hyscan_param_list_get_double (list, TIMEOUT_ERROR_PARAM);

        /* Параметры UART подключения. */
        if (hyscan_param_list_contains (list, UART_PORT_PARAM))
          priv->uart_port = hyscan_param_list_get_enum (list, UART_PORT_PARAM);
        if (hyscan_param_list_contains (list, UART_MODE_PARAM))
          priv->uart_mode = hyscan_param_list_get_enum (list, UART_MODE_PARAM);

        /* Параметры UDP подключения. */
        if (hyscan_param_list_contains (list, UDP_ADDRESS_PARAM))
          priv->udp_address = hyscan_param_list_get_enum (list, UDP_ADDRESS_PARAM);
        if (hyscan_param_list_contains (list, UDP_PORT_PARAM))
          priv->udp_port = hyscan_param_list_get_integer (list, UDP_PORT_PARAM);
      }
      break;

    default:
      {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
      break;
    }
}

static void
hyscan_nmea_driver_object_constructed (GObject *object)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (object);
  HyScanNmeaDriverPrivate *priv = driver->priv;

  GRand *rand;
  gchar *dev_id;

  /* По умолчанию приём данных включен. */
  priv->enable = TRUE;

  /* Начальный статус. */
  priv->status = NMEA_DRIVER_STATUS_ERROR;
  priv->prev_status = NMEA_DRIVER_STATUS_ERROR;

  /* Таймер данных. */
  priv->data_timer = g_timer_new ();

  /* Буфер данных. */
  priv->buffer = hyscan_buffer_new ();

  /* Название датчика. */
  priv->name = g_strdup_printf ("nmea%d", driver_id);

  /* Автоматический выбор UART порта и режима работы. */
  if ((g_strcmp0 (priv->uri, HYSCAN_NMEA_DRIVER_UART_URI) == 0) && (priv->uart_port == 0))
    priv->scanner = g_thread_new ("uart-scanner", hyscan_nmea_driver_scanner, driver);

  /* Конкретный UART или UDP порт. */
  else
    priv->starter = g_thread_new ("uart-starter", hyscan_nmea_driver_starter, driver);

  /* Уникальный идентификатор устройства. */
  rand = g_rand_new ();
  dev_id = g_strdup_printf ("%s.%04d", priv->name, g_rand_int_range (rand, 1000, 9999));
  priv->state_prefix = g_strdup_printf ("/state/%s/", dev_id);

  /* Схема датчика. */
  priv->sensor_schema = hyscan_nmea_driver_create_schema (priv->name, dev_id);

  g_rand_free (rand);
  g_free (dev_id);
}

static void
hyscan_nmea_driver_object_finalize (GObject *object)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (object);
  HyScanNmeaDriverPrivate *priv = driver->priv;

  g_atomic_int_set (&priv->shutdown, TRUE);
  g_clear_pointer (&priv->starter, g_thread_join);
  g_clear_pointer (&priv->scanner, g_thread_join);

  g_clear_object (&priv->transport);

  g_object_unref (priv->buffer);
  g_object_unref (priv->sensor_schema);
  g_clear_object (&priv->connect_schema);

  g_timer_destroy (priv->data_timer);

  g_free (priv->state_prefix);
  g_free (priv->name);
  g_free (priv->uri);

  G_OBJECT_CLASS (hyscan_nmea_driver_parent_class)->finalize (object);
}

/* Функция создаёт схему датчика. */
static HyScanDataSchema *
hyscan_nmea_driver_create_schema (const gchar *name,
                                  const gchar *dev_id)
{
  HyScanDataSchemaBuilder *builder;
  HyScanSensorSchema *sensor;
  HyScanDataSchema *schema;
  gchar *key_id;
  gchar *data;

  builder = hyscan_data_schema_builder_new ("sensor");
  sensor = hyscan_sensor_schema_new (builder);

  /* Описание датчика. */
  hyscan_sensor_schema_add_sensor (sensor, name, dev_id, _ ("NMEA sensor"));

  /* Признак доступности датчика. */
  key_id = g_strdup_printf ("/state/%s/enable", dev_id);
  hyscan_data_schema_builder_key_boolean_create (builder, key_id, _ ("Enable"), NULL, FALSE);
  hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  /* Статус работы датчика. */
  key_id = g_strdup_printf ("/state/%s/status", dev_id);
  hyscan_data_schema_builder_key_string_create (builder, key_id, _ ("Status"), NULL, "error");
  hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  data = hyscan_data_schema_builder_get_data (builder);
  schema = hyscan_data_schema_new_from_string (data, "sensor");

  g_object_unref (sensor);
  g_object_unref (builder);
  g_free (data);

  return schema;
}

/* Поток подключения к NMEA датчикам. */
static gpointer
hyscan_nmea_driver_starter (gpointer user_data)
{
  HyScanNmeaDriver *driver = user_data;
  HyScanNmeaDriverPrivate *priv = driver->priv;

  while (!g_atomic_int_get (&priv->shutdown))
    {
      /* Подключение установлено - проверяем приём данных. */
      if (g_atomic_pointer_get (&priv->transport) != NULL)
        {
          hyscan_nmea_driver_check_data (driver);
        }

      /* Определённый UART порт. */
      else if (g_strcmp0 (priv->uri, HYSCAN_NMEA_DRIVER_UART_URI) == 0)
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

              if (port_id == priv->uart_port)
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

              if (!hyscan_nmea_uart_set_device (uart, uart_path, priv->uart_mode))
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
      else if (g_strcmp0 (priv->uri, HYSCAN_NMEA_DRIVER_UDP_URI) == 0)
        {
          HyScanNmeaUDP *udp;
          gchar *address = NULL;

          /* Выбраны все адреса. */
          if (priv->udp_address == 0)
            {
              address = g_strdup ("any");
            }

          /* Loopback адрес. */
          else if (priv->udp_address == 1)
            {
              address = g_strdup ("loopback");
            }

          /* Ищем выбранный адрес под его идентификатору. */
          else
            {
              gchar **addresses = hyscan_nmea_udp_list_addresses ();
              guint i;

              for (i = 0; (addresses != NULL) && (addresses[i] != NULL); i++)
                {
                  guint address_id = g_str_hash (addresses[i]);

                  if (address_id == priv->udp_address)
                    address = g_strdup (addresses [i]);
                }

              g_strfreev (addresses);
            }

          if (address != NULL)
            {
              udp = hyscan_nmea_udp_new ();

              if (!hyscan_nmea_udp_set_address (udp, address, priv->udp_port))
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

  g_timer_destroy (timer);

  return NULL;
}

/* Функция проверяет приём данных и перезапускает порт при необходимости. */
static void
hyscan_nmea_driver_check_data (HyScanNmeaDriver *driver)
{
  HyScanNmeaDriverPrivate *priv = driver->priv;

  gdouble data_timeout = g_timer_elapsed (priv->data_timer, NULL);
  gint cur_status = g_atomic_int_get (&priv->status);
  gboolean io_error = FALSE;

  /* Ошибка ввода/вывода - перезапускаем порт. */
  if (g_atomic_int_get (&priv->io_error))
    {
      g_object_unref (priv->transport);
      g_atomic_pointer_set (&priv->transport, NULL);

      g_atomic_int_set (&priv->status, NMEA_DRIVER_STATUS_ERROR);
      g_atomic_int_set (&priv->io_error, FALSE);

      cur_status = NMEA_DRIVER_STATUS_ERROR;
      io_error = TRUE;
    }

  /* Данных нет длительное время. */
  else if (data_timeout > priv->error_timeout)
    {
      g_atomic_int_set (&priv->status, NMEA_DRIVER_STATUS_ERROR);
      cur_status = NMEA_DRIVER_STATUS_ERROR;
    }

  /* Посылаем предупреждение. */
  else if (data_timeout > priv->warning_timeout)
    {
      gboolean changed;
      changed = g_atomic_int_compare_and_exchange (&priv->status,
                                                   NMEA_DRIVER_STATUS_OK,
                                                   NMEA_DRIVER_STATUS_WARNING);
      if (changed)
        cur_status = NMEA_DRIVER_STATUS_WARNING;
    }

  /* Изменился статус. */
  if (g_atomic_int_get (&priv->prev_status) != cur_status)
    {
      gchar message[256];

      if (cur_status == NMEA_DRIVER_STATUS_OK)
        {
          g_snprintf (message, sizeof (message),
                      _ ("The sensor is fully operational."));
        }
      else if (cur_status == NMEA_DRIVER_STATUS_WARNING)
        {
          g_snprintf (message, sizeof (message),
                      _ ("Temporary error while receiving data."));
        }
      else
        {
          g_snprintf (message, sizeof (message),
                      _ ("An error occurred while receiving data%s"),
                      io_error ? ", port disconnected." : ".");
        }

      g_signal_emit_by_name (driver, "sensor-log",
                                     priv->name,
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
  gchar **nmea;
  guint i;

  /* Сбрасываем таймер таймаута данных. */
  g_timer_start (priv->data_timer);

  /* Сигнализируем о приёме данных. */
  g_atomic_int_set (&priv->status, NMEA_DRIVER_STATUS_OK);

  /* Приём данных отключен. */
  if (!priv->enable)
    return;

  /* Отправка всех NMEA данных. */
  hyscan_buffer_wrap_data (priv->buffer, HYSCAN_DATA_STRING, (gpointer)data, size);
  g_signal_emit_by_name (driver, "sensor-data", priv->name, HYSCAN_SOURCE_NMEA_ANY, time, priv->buffer);

  /* Отправка RMC, GGA и DPT. */
  nmea = g_strsplit_set (data, "\r\n", size);
  for (i = 0; (nmea != NULL) && (nmea[i] != NULL); i++)
    {
      if (g_str_has_prefix (nmea[i], "$GPRMC"))
        {
          hyscan_buffer_wrap_data (priv->buffer, HYSCAN_DATA_STRING, nmea[i], strlen (nmea[i]) + 1);
          g_signal_emit_by_name (driver, "sensor-data", priv->name, HYSCAN_SOURCE_NMEA_RMC, time, priv->buffer);
        }
      else if (g_str_has_prefix (nmea[i], "$GPGGA"))
        {
          hyscan_buffer_wrap_data (priv->buffer, HYSCAN_DATA_STRING, nmea[i], strlen (nmea[i]) + 1);
          g_signal_emit_by_name (driver, "sensor-data", priv->name, HYSCAN_SOURCE_NMEA_GGA, time, priv->buffer);
        }
      else if (g_str_has_prefix (nmea[i], "$GPDPT"))
        {
          hyscan_buffer_wrap_data (priv->buffer, HYSCAN_DATA_STRING, nmea[i], strlen (nmea[i]) + 1);
          g_signal_emit_by_name (driver, "sensor-data", priv->name, HYSCAN_SOURCE_NMEA_DPT, time, priv->buffer);
        }
    }

  g_strfreev (nmea);
}

static HyScanDataSchema *
hyscan_nmea_driver_param_schema (HyScanParam *param)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (param);
  HyScanNmeaDriverPrivate *priv = driver->priv;

  return g_object_ref (priv->sensor_schema);
}

static gboolean
hyscan_nmea_driver_param_get (HyScanParam      *param,
                              HyScanParamList  *list)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (param);
  HyScanNmeaDriverPrivate *priv = driver->priv;

  const gchar * const *params;
  guint i;

  params = hyscan_param_list_params (list);
  if (params == NULL)
    return FALSE;

  for (i = 0; params[i] != NULL; i++)
    {
      const gchar *key_id;

      /* Обрабатываем только параметры из ветки /state. */
      if (!g_str_has_prefix (params[i], priv->state_prefix))
        return FALSE;

      key_id = params[i] + strlen (priv->state_prefix);

      /* Параметр enable. */
      if (g_strcmp0 (key_id, "enable") == 0)
        {
          gboolean enable = (priv->transport != NULL) ? TRUE : FALSE;

          hyscan_param_list_set_boolean (list, params[i], enable);
        }

      /* Параметр status. */
      else if (g_strcmp0 (key_id, "status") == 0)
        {
          const gchar *status;

          if (g_atomic_int_get (&priv->status) == NMEA_DRIVER_STATUS_OK)
            status = "ok";
          else if (g_atomic_int_get (&priv->status) == NMEA_DRIVER_STATUS_WARNING)
            status = "warning";
          else
            status = "error";

          hyscan_param_list_set_string (list, params[i], status);
        }

      /* Неизвестный параметр. */
      else
        {
          return FALSE;
        }
    }

  return TRUE;
}

static gboolean
hyscan_nmea_driver_sensor_set_sound_velocity (HyScanSensor *sensor,
                                              GList        *svp)
{
  return TRUE;
}

static gboolean
hyscan_nmea_driver_sensor_set_enable (HyScanSensor *sensor,
                                      const gchar  *name,
                                      gboolean      enable)
{
  HyScanNmeaDriver *driver = HYSCAN_NMEA_DRIVER (sensor);
  HyScanNmeaDriverPrivate *priv = driver->priv;

  if (g_strcmp0 (priv->name, name) != 0)
    return FALSE;

  g_atomic_int_set (&priv->enable, enable);

  return TRUE;
}

/**
 * hyscan_nmea_driver_new:
 * @uri: путь к датчику
 * @params: параметры подключения датчика
 *
 * Функция создаёт новый объект #HyScanNmeaDriver.
 *
 * Returns: #HyScanNmeaDriver. Для удаления #g_object_unref.
 */
HyScanNmeaDriver *
hyscan_nmea_driver_new (const gchar     *uri,
                        HyScanParamList *params)
{
  return g_object_new (HYSCAN_TYPE_NMEA_DRIVER,
                       "uri", uri,
                       "params", params,
                       NULL);
}

/**
 * hyscan_nmea_driver_get_uart_schema:
 * @uri: путь к датчику
 *
 * Функция возвращает схему параметров подключения к NMEA датчику.
 *
 * Returns: #HyScanDataSchema. Для удаления #g_object_unref.
 */
HyScanDataSchema *
hyscan_nmea_driver_get_connect_schema (const gchar *uri)
{
  HyScanDataSchemaBuilder *builder;
  HyScanDataSchema *schema;
  gchar *data;
  gchar *URI;

  URI = g_ascii_strup (uri, -1);

  builder = hyscan_data_schema_builder_new ("params");

  /* Таймауты приёма данных. */
  hyscan_data_schema_builder_key_double_create (builder, TIMEOUT_WARNING_PARAM,
                                                _ ("Timeout before warning"), NULL, 5.0);
  hyscan_data_schema_builder_key_double_range  (builder, TIMEOUT_WARNING_PARAM,
                                                0.0, 30.0, 1.0);

  hyscan_data_schema_builder_key_double_create (builder, TIMEOUT_ERROR_PARAM,
                                                _ ("Timeout before error"), NULL, 30.0);
  hyscan_data_schema_builder_key_double_range  (builder, TIMEOUT_ERROR_PARAM,
                                                30.0, 60.0, 1.0);

  /* Параметры UART порта. */
  if (g_str_has_prefix (URI, HYSCAN_NMEA_DRIVER_UART_URI))
    {
      GList *devices, *device;

      /* Список UART портов. */
      hyscan_data_schema_builder_enum_create (builder, "uart-port");

      hyscan_data_schema_builder_enum_value_create (builder, "uart-port", 0,
                                                    _ ("Auto select"), NULL);

      device = devices = hyscan_nmea_uart_list_devices ();
      while (device != NULL)
        {
          HyScanNmeaUARTDevice *info = device->data;
          guint port_id = g_str_hash (info->path);

          hyscan_data_schema_builder_enum_value_create (builder, "uart-port", port_id,
                                                        info->name, NULL);

          device = g_list_next (device);
        }
      g_list_free_full (devices, (GDestroyNotify)hyscan_nmea_uart_device_free);

      hyscan_data_schema_builder_key_enum_create (builder, UART_PORT_PARAM,
                                                  _ ("Port"), NULL,
                                                  "uart-port", 0);

      /* Режимы работы UART порта. */
      hyscan_data_schema_builder_enum_create (builder, "uart-mode");

      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode", HYSCAN_NMEA_UART_MODE_AUTO,
                                                    _ ("Auto select"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode", HYSCAN_NMEA_UART_MODE_4800_8N1,
                                                    _ ("4800 8N1"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode", HYSCAN_NMEA_UART_MODE_9600_8N1,
                                                    _ ("9600 8N1"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode", HYSCAN_NMEA_UART_MODE_19200_8N1,
                                                    _ ("19200 8N1"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode", HYSCAN_NMEA_UART_MODE_38400_8N1,
                                                    _ ("38400 8N1"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode", HYSCAN_NMEA_UART_MODE_57600_8N1,
                                                    _ ("57600 8N1"), NULL);
      hyscan_data_schema_builder_enum_value_create (builder, "uart-mode", HYSCAN_NMEA_UART_MODE_115200_8N1,
                                                    _ ("115200 8N1"), NULL);

      hyscan_data_schema_builder_key_enum_create (builder, UART_MODE_PARAM,
                                                  _ ("Mode"), NULL,
                                                  "uart-mode", HYSCAN_NMEA_UART_MODE_AUTO);
    }

  /* Параметры UDP порта. */
  else if (g_str_has_prefix (URI, HYSCAN_NMEA_DRIVER_UDP_URI))
    {
      gchar **addresses;
      guint i;

      /* Список IP адресов. */
      hyscan_data_schema_builder_enum_create (builder, "udp-address");

      hyscan_data_schema_builder_enum_value_create (builder, "udp-address", 0,
                                                    _ ("All addresses"), NULL);

      addresses = hyscan_nmea_udp_list_addresses ();
      for (i = 0; (addresses != NULL) && (addresses[i] != NULL); i++)
        {
          guint address_id = g_str_hash (addresses[i]);

          hyscan_data_schema_builder_enum_value_create (builder, "udp-address", address_id,
                                                        addresses[i], NULL);
        }
      g_strfreev (addresses);

      hyscan_data_schema_builder_key_enum_create (builder, UDP_ADDRESS_PARAM,
                                                  _ ("Address"), NULL,
                                                  "udp-address", 0);

      /* UDP/IP порт. */
      hyscan_data_schema_builder_key_integer_create (builder, UDP_PORT_PARAM,
                                                     _ ("UDP port"), NULL, 10000);
      hyscan_data_schema_builder_key_integer_range  (builder, UDP_PORT_PARAM,
                                                     1024, 65535, 1);
    }

  data = hyscan_data_schema_builder_get_data (builder);
  schema = hyscan_data_schema_new_from_string (data, "params");

  g_object_unref (builder);
  g_free (data);
  g_free (URI);

  return schema;
}

static void
hyscan_nmea_driver_param_interface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_nmea_driver_param_schema;
  iface->set = NULL;
  iface->get = hyscan_nmea_driver_param_get;
}

static void
hyscan_nmea_driver_sensor_interface_init (HyScanSensorInterface *iface)
{
  iface->set_sound_velocity = hyscan_nmea_driver_sensor_set_sound_velocity;
  iface->set_enable = hyscan_nmea_driver_sensor_set_enable;
}
