/* hyscan-nmea-uart.c
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

/**
 * SECTION: hyscan-nmea-uart
 * @Short_description: класс приёма NMEA данных через UART порты
 * @Title: HyScanNmeaUART
 *
 * Класс предназначен для приёма NMEA данных через UART порты. Класс
 * наследуется от #HyScanNmeaReceiver.
 *
 * Объект HyScanNmeaUART создаётся с помощию функции #hyscan_nmea_uart_new.
 * Устройство для приёма данных и его параметры задаются с помощью функции
 * #hyscan_nmea_uart_set_device.
 *
 * Если выбран режим автоматического определения скорости UART порта -
 * #HYSCAN_NMEA_UART_MODE_AUTO, принимаются только корректные NMEA строки.
 *
 * Список UART портов, доступных в системе, можно получить с помощью функции
 * #hyscan_nmea_uart_list_devices.
 */

#include "hyscan-nmea-uart.h"
#include "hyscan-nmea-marshallers.h"

#ifdef G_OS_UNIX
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

#define HANDLE gint
#define INVALID_HANDLE_VALUE -1
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#include <tchar.h>

#define sscanf sscanf_s
#endif

#define N_CHARS_TIMEOUT  25
#define N_BUFFERS        16
#define MAX_MSG_SIZE     4084

enum
{
  PROP_O,
  PROP_NAME,
};

typedef struct
{
  HANDLE               fd;             /* Дескриптор открытого порта. */
  gdouble              timeout;        /* Таймаут при чтении, с. */
} UARTDevice;

struct _HyScanNmeaUARTPrivate
{
  GThread             *receiver;       /* Поток приёма данных из UART порта. */

  gboolean             started;        /* Признак работы потока приёма данных. */
  gboolean             configure;      /* Признак режима конфигурации UART порта. */
  gboolean             terminate;      /* Признак необходимости завершения работы. */

  UARTDevice          *device;         /* Параметры UART устройства. */
  gboolean             auto_speed;     /* Признак автоматического выбора скорости приёма. */
};

static void            hyscan_nmea_uart_object_constructed     (GObject               *object);
static void            hyscan_nmea_uart_object_finalize        (GObject               *object);

static UARTDevice *    hyscan_nmea_uart_open                   (const gchar           *path);
static void            hyscan_nmea_uart_close                  (UARTDevice            *device);

static gboolean        hyscan_nmea_uart_set_mode               (UARTDevice            *device,
                                                                HyScanNmeaUARTMode     mode);

static gchar           hyscan_nmea_uart_read                   (UARTDevice            *device,
                                                                HyScanNmeaUART        *uart);

static gpointer        hyscan_nmea_uart_receiver               (gpointer               user_data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanNmeaUART, hyscan_nmea_uart, HYSCAN_TYPE_NMEA_RECEIVER)

static void
hyscan_nmea_uart_class_init (HyScanNmeaUARTClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_nmea_uart_object_constructed;
  object_class->finalize = hyscan_nmea_uart_object_finalize;
}

static void
hyscan_nmea_uart_init (HyScanNmeaUART *uart)
{
  uart->priv = hyscan_nmea_uart_get_instance_private (uart);
}

static void
hyscan_nmea_uart_object_constructed (GObject *object)
{
  HyScanNmeaUART *uart = HYSCAN_NMEA_UART (object);
  HyScanNmeaUARTPrivate *priv = uart->priv;

  G_OBJECT_CLASS (hyscan_nmea_uart_parent_class)->constructed (object);

  priv->started = TRUE;

  priv->receiver = g_thread_new ("uart-receiver", hyscan_nmea_uart_receiver, uart);
}

static void
hyscan_nmea_uart_object_finalize (GObject *object)
{
  HyScanNmeaUART *uart = HYSCAN_NMEA_UART (object);
  HyScanNmeaUARTPrivate *priv = uart->priv;

  g_atomic_int_set (&priv->terminate, TRUE);
  g_thread_join (priv->receiver);

  hyscan_nmea_uart_close (priv->device);

  G_OBJECT_CLASS (hyscan_nmea_uart_parent_class)->finalize (object);
}

/* UNIX версии функций работы с uart портами. */
#ifdef G_OS_UNIX

/* Функция открывает uart порт. */
static UARTDevice *
hyscan_nmea_uart_open (const gchar *path)
{
  UARTDevice *device;
  HANDLE fd;

  fd = open (path, O_RDONLY | O_NOCTTY | O_NDELAY | O_NONBLOCK);
  if (fd < 0)
    return NULL;

  device = g_slice_new0 (UARTDevice);
  device->fd = fd;

  return device;
}

/* Функция закрывает uart порт. */
static void
hyscan_nmea_uart_close (UARTDevice *device)
{
  if ((device != NULL) && (device->fd != INVALID_HANDLE_VALUE))
    close (device->fd);

  g_slice_free (UARTDevice, device);
}

/* Функция устанавливает режим работы UART порта - unix версия. */
static gboolean
hyscan_nmea_uart_set_mode (UARTDevice         *device,
                           HyScanNmeaUARTMode  mode)
{
  struct termios options = {0};
  gdouble baudrate = 0;

  if ((device == NULL) || (device->fd == INVALID_HANDLE_VALUE))
    return FALSE;

  /* Выбор режима работы. */
  switch (mode)
    {
    case HYSCAN_NMEA_UART_MODE_AUTO:
      device->timeout = 0;
      return TRUE;

    case HYSCAN_NMEA_UART_MODE_4800_8N1:
      baudrate = 1.0 / 600;
      options.c_cflag = B4800;
      break;

    case HYSCAN_NMEA_UART_MODE_9600_8N1:
      baudrate = 1.0 / 1200;
      options.c_cflag = B9600;
      break;

    case HYSCAN_NMEA_UART_MODE_19200_8N1:
      baudrate = 1.0 / 2400;
      options.c_cflag = B19200;
      break;

    case HYSCAN_NMEA_UART_MODE_38400_8N1:
      baudrate = 1.0 / 4800;
      options.c_cflag = B38400;
      break;

    case HYSCAN_NMEA_UART_MODE_57600_8N1:
      baudrate = 1.0 / 7200;
      options.c_cflag = B57600;
      break;

    case HYSCAN_NMEA_UART_MODE_115200_8N1:
      baudrate = 1.0 / 14400;
      options.c_cflag = B115200;
      break;

    default:
      return FALSE;
    }

  /* Устанавливаем параметры устройства. */
  cfmakeraw (&options);
  if (tcflush (device->fd, TCIFLUSH) != 0)
    return FALSE;
  if (tcsetattr (device->fd, TCSANOW, &options) != 0)
    return FALSE;

  /* Таймаут на N_CHARS_TIMEOUT символов. */
  device->timeout = baudrate * N_CHARS_TIMEOUT;

  return TRUE;
}

/* Функция считывает доступные данные из uart порта. */
static gchar
hyscan_nmea_uart_read (UARTDevice     *device,
                       HyScanNmeaUART *uart)
{
  fd_set set;
  struct timeval tv;
  gchar data;

  if ((device == NULL) || (device->fd == INVALID_HANDLE_VALUE))
    return 0;

  /* Ожидаем новые данные в течение device->timeout. */
  FD_ZERO (&set);
  tv.tv_sec = 0;
  tv.tv_usec = 100000 * device->timeout;
  FD_SET (device->fd, &set);
  if (select (device->fd + 1, &set, NULL, NULL, &tv) <= 0)
    return  0;

  /* Считываем данные. */
  if (read (device->fd, &data, 1) <= 0)
    {
      /* При ошибке чтения блокируем работу на 100 мс и посылаем сигнал "nmea-io-error". */
      if (errno)
        {
          hyscan_nmea_receiver_io_error (HYSCAN_NMEA_RECEIVER (uart));
          g_usleep (100000);
        }

      return 0;
    }

  return data;
}
#endif

/* Windows версии функций работы с uart портами. */
#ifdef G_OS_WIN32
/* Функция открывает uart порт. */
static UARTDevice *
hyscan_nmea_uart_open (const gchar *path)
{
  UARTDevice *device;
  HANDLE fd;

  fd = CreateFile (path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
  if (fd == INVALID_HANDLE_VALUE)
    return NULL;

  device = g_slice_new0 (UARTDevice);
  device->fd = fd;

  return device;
}

/* Функция закрывает uart порт. */
static void
hyscan_nmea_uart_close (UARTDevice *device)
{
  if ((device != NULL) && (device->fd != INVALID_HANDLE_VALUE))
    CloseHandle (device->fd);

  g_slice_free (UARTDevice, device);
}

/* Функция устанавливает режим работы UART порта. */
static gboolean
hyscan_nmea_uart_set_mode (UARTDevice         *device,
                           HyScanNmeaUARTMode  mode)
{
  COMMTIMEOUTS cto = {0};
  DCB dcb = {0};

  gdouble baudrate = 0;

  if ((device == NULL) || (device->fd == INVALID_HANDLE_VALUE))
    return FALSE;

  /* Выбор режима работы. */
  switch (mode)
    {
    case HYSCAN_NMEA_UART_MODE_AUTO:
      device->timeout = 0;
      return TRUE;

    case HYSCAN_NMEA_UART_MODE_4800_8N1:
      baudrate = 1.0 / 600;
      BuildCommDCB ("4800,n,8,1", &dcb);
      break;

    case HYSCAN_NMEA_UART_MODE_9600_8N1:
      baudrate = 1.0 / 1200;
      BuildCommDCB ("9600,n,8,1", &dcb);
      break;

    case HYSCAN_NMEA_UART_MODE_19200_8N1:
      baudrate = 1.0 / 2400;
      BuildCommDCB ("19200,n,8,1", &dcb);
      break;

    case HYSCAN_NMEA_UART_MODE_38400_8N1:
      baudrate = 1.0 / 4800;
      BuildCommDCB ("38400,n,8,1", &dcb);
      break;

    case HYSCAN_NMEA_UART_MODE_57600_8N1:
      baudrate = 1.0 / 7200;
      BuildCommDCB ("57600,n,8,1", &dcb);
      break;

    case HYSCAN_NMEA_UART_MODE_115200_8N1:
      baudrate = 1.0 / 14400;
      BuildCommDCB ("115200,n,8,1", &dcb);
      break;

    default:
      return FALSE;
    }

  if (!SetCommState (device->fd, &dcb))
    return FALSE;

  cto.ReadIntervalTimeout = MAXDWORD;
  cto.ReadTotalTimeoutMultiplier = MAXDWORD;
  cto.ReadTotalTimeoutConstant = 1000 * baudrate;
  if (!SetCommTimeouts (device->fd, &cto))
    return FALSE;

  /* Таймаут на N_CHARS_TIMEOUT символов. */
  device->timeout = baudrate * N_CHARS_TIMEOUT;

  return TRUE;
}

/* Функция считывает доступные данные из uart порта. */
static gchar
hyscan_nmea_uart_read (UARTDevice     *device,
                       HyScanNmeaUART *uart)
{
  gchar data;
  DWORD readed = -1;

  if ((device == NULL) || (device->fd == INVALID_HANDLE_VALUE))
    return 0;

  if (!ReadFile (device->fd, &data, 1, &readed, NULL) || (readed == 0))
    {
      /* При ошибке чтения посылаем сигнал "nmea-io-error". */
      if (GetLastError ())
        hyscan_nmea_receiver_io_error (HYSCAN_NMEA_RECEIVER (uart));

      return 0;
    }

  return data;
}
#endif

/* Поток приёма данных. */
static gpointer
hyscan_nmea_uart_receiver (gpointer user_data)
{
  HyScanNmeaUART *uart = user_data;
  HyScanNmeaReceiver *nmea = user_data;
  HyScanNmeaUARTPrivate *priv = uart->priv;

  HyScanNmeaUARTMode cur_mode = HYSCAN_NMEA_UART_MODE_DISABLED;
  GTimer *timer = g_timer_new ();

  while (!g_atomic_int_get (&priv->terminate))
    {
      gint64 rx_time;
      gchar rx_data;

      /* Режим конфигурации. */
      if (g_atomic_int_get (&priv->configure))
        {
          /* Восстанавливаем режим работы и закрываем устройство. */
          g_clear_pointer (&priv->device, hyscan_nmea_uart_close);
          cur_mode = HYSCAN_NMEA_UART_MODE_DISABLED;

          /* Ждём завершения конфигурации. */
          g_atomic_int_set (&priv->started, FALSE);
          g_usleep (100000);
          continue;
        }

      /* Конфигурация завершена. */
      else
        {
          /* Устройство не выбрано. */
          if (priv->device == NULL)
            {
              g_usleep (100000);
              continue;
            }

          /* В автоматическом режиме переключаем режимы работы каждые 2 секунды,
           * до тех пор пока не найдём рабочий. */
          if ((priv->auto_speed) &&
              ((cur_mode == HYSCAN_NMEA_UART_MODE_DISABLED) ||
              (g_timer_elapsed (timer, NULL) > 2.0)))
            {
              if ((cur_mode == HYSCAN_NMEA_UART_MODE_DISABLED) ||
                  (cur_mode == HYSCAN_NMEA_UART_MODE_115200_8N1))
                {
                  cur_mode = HYSCAN_NMEA_UART_MODE_4800_8N1;
                }
              else
                {
                  cur_mode += 1;
                }

              hyscan_nmea_uart_set_mode (priv->device, cur_mode);
              g_timer_start (timer);
            }
        }

      /* Пытаемся прочитать данные из порта. */
      rx_data = hyscan_nmea_uart_read (priv->device, uart);
      rx_time = g_get_monotonic_time ();

      /* Отправляем данные на обработку. */
      if (rx_data > 0)
        {
          if (hyscan_nmea_receiver_add_data (nmea, rx_time, &rx_data, 1))
            g_timer_start (timer);
        }
      else
        {
          hyscan_nmea_receiver_flush (nmea, priv->device->timeout);
        }
    }

  g_timer_destroy (timer);

  return NULL;
}

/**
 * hyscan_nmea_uart_new:
 *
 * Функция создаёт новый объект #HyScanNmeaUART.
 *
 * Returns: #HyScanNmeaUART. Для удаления #g_object_unref.
 */
HyScanNmeaUART *
hyscan_nmea_uart_new ()
{
  return g_object_new (HYSCAN_TYPE_NMEA_UART, NULL);
}

/**
 * hyscan_nmea_uart_set_device:
 * @uart: указатель на #HyScanNmeaUART
 * @path: путь к устройству
 * @mode: режим работы
 *
 * Функция выбирает используемое UART устройство и режим его работы.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_nmea_uart_set_device (HyScanNmeaUART     *uart,
                             const gchar        *path,
                             HyScanNmeaUARTMode  mode)
{
  HyScanNmeaReceiver *nmea = HYSCAN_NMEA_RECEIVER (uart);
  HyScanNmeaUARTPrivate *priv;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_UART (uart), FALSE);

  priv = uart->priv;

  /* Переходим в режим конфигурации. */
  while (!g_atomic_int_compare_and_exchange (&priv->configure, FALSE, TRUE))
    g_usleep (10000);
  while (g_atomic_int_get (&priv->started))
    g_usleep (10000);

  /* Устройство отключено. */
  if (path == NULL || mode == HYSCAN_NMEA_UART_MODE_DISABLED)
    {
      status = TRUE;
      goto exit;
    }

  /* Открываем устройство. */
  priv->device = hyscan_nmea_uart_open (path);
  if (priv->device == NULL)
    {
      g_warning ("HyScanNmeaUART: %s: can't open device", path);
      goto exit;
    }

  /* В автоматическом режиме отключается приём "плохих" строк. */
  if (mode == HYSCAN_NMEA_UART_MODE_AUTO)
    {
      priv->auto_speed = TRUE;
      hyscan_nmea_receiver_skip_broken (nmea, TRUE);
    }
  else
    {
      priv->auto_speed = FALSE;
    }

  /* Устанавливаем режим работы порта. */
  if (!hyscan_nmea_uart_set_mode (priv->device, mode))
    {
      g_clear_pointer (&priv->device, hyscan_nmea_uart_close);
      g_warning ("HyScanNmeaUART: %s: can't set device mode", path);
      goto exit;
    }

  status = TRUE;

exit:
  /* Завершаем конфигурацию. */
  g_atomic_int_set (&priv->started, TRUE);
  g_atomic_int_set (&priv->configure, FALSE);

  return status;
}

/**
 * hyscan_nmea_uart_list_devices:
 *
 * Функция возвращает список UART устройств.
 *
 * Память выделенная под список должна быть освобождена после использования
 * функцией #g_list_free_full. Для освобождения элементов списка необходимо
 * использовать функцию #hyscan_nmea_uart_device_free.
 *
 * Returns: (element-type HyScanNmeaUARTDevice) (transfer full): Список UART
 * устройств или NULL.
 */

#ifdef G_OS_UNIX
GList *
hyscan_nmea_uart_list_devices (void)
{
  GList *list = NULL;

  GDir *dir;
  const gchar *device;

  /* Список всех устройств. */
  dir = g_dir_open ("/dev", 0, NULL);
  if (dir == NULL)
    return NULL;

  while ((device = g_dir_read_name (dir)) != NULL)
    {
      struct termios options;
      gboolean usb_serial;
      gboolean serial;
      gchar *path;
      int fd;

      /* Пропускаем элементы "." и ".." */
      if (device[0] =='.' && device[1] == 0)
        continue;
      if (device[0] =='.' && device[1] =='.' && device[2] == 0)
        continue;

      /* Пропускаем все устройства, имена которых не начинаются на ttyS или ttyUSB. */
      serial = g_str_has_prefix (device, "ttyS");
      usb_serial = g_str_has_prefix (device, "ttyUSB");
      if (!serial && !usb_serial)
        continue;

      /* Открываем устройство и проверяем его тип. */
      path = g_strdup_printf ("/dev/%s", device);
      fd = open (path, O_RDWR | O_NONBLOCK | O_NOCTTY);
      g_free (path);

      /* Если не смогли открыть файл, возможно у нас нет на это прав. */
      if (fd < 0)
        continue;

      /* Проверяем тип устройства. */
      if (tcgetattr (fd, &options) == 0)
        {
          HyScanNmeaUARTDevice *port;
          gint index;

          if (serial)
            index = g_ascii_strtoll(device + 4, NULL, 10);
          else
            index = g_ascii_strtoll(device + 6, NULL, 10);

          port = g_slice_new (HyScanNmeaUARTDevice);
          port->name = g_strdup_printf ("%s%d", serial ? "COM" : "USBCOM", index + 1);
          port->path = g_strdup_printf ("/dev/%s", device);

          list = g_list_prepend (list, port);
        }

      close (fd);
    }

  g_dir_close (dir);

  return list;
}
#endif

#ifdef G_OS_WIN32
GList *
hyscan_nmea_uart_list_devices (void)
{
  GList *list = NULL;

  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_data;
  DWORD dev_index;

  /* Список всех устройств типа UART. */
  dev_info = SetupDiGetClassDevs ((const GUID *)&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
  dev_data.cbSize = sizeof (SP_DEVINFO_DATA);
  dev_index = 0;

  while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_data))
    {
      HKEY hkey;
      LONG status;
      gchar port_path[1024] = {0};
      gchar port_name[1024] = {0};
      DWORD port_path_length = sizeof (port_path);
      DWORD port_name_length = sizeof (port_name);

      HyScanNmeaUARTDevice *port;
      gboolean is_usb = FALSE;

      dev_index += 1;

      /* Путь к порту из реестра. */
      hkey = SetupDiOpenDevRegKey (dev_info, &dev_data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
      status = RegQueryValueEx (hkey, _T ("PortName"), NULL, NULL, (LPBYTE)port_path, &port_path_length);
      RegCloseKey(hkey);

      if (status != EXIT_SUCCESS)
        continue;

      /* Пропускаем LPT порты. */
      if (g_str_has_prefix (port_path, "LPT"))
        continue;

      /* Описание порта. */
      status = SetupDiGetDeviceRegistryProperty (dev_info, &dev_data,
                                                 SPDRP_FRIENDLYNAME, NULL, (PBYTE)port_name,
                                                 port_name_length, &port_name_length);
      if (!status)
        port_name_length = 0;

      if ((g_strstr_len (port_name, port_name_length, "USB") != NULL) ||
          (g_strstr_len (port_name, port_name_length, "usb") != NULL))
        {
          is_usb = TRUE;
        }

      port = g_slice_new (HyScanNmeaUARTDevice);
      port->path = g_strdup_printf ("%s:", port_path);

      if (is_usb)
        port->name = g_strdup_printf("USB%s", port_path);
      else
        port->name = g_strdup_printf("%s", port_path);

      list = g_list_prepend (list, port);
  }

  return list;
}
#endif

/**
 * hyscan_nmea_uart_device_copy:
 * @device: структура #HyScanNmeaUARTDevice для копирования
 *
 * Функция создаёт копию структуры #HyScanNmeaUARTDevice.
 *
 * Returns: (transfer full): Новая структура #HyScanNmeaUARTDevice.
 * Для удаления #hyscan_nmea_uart_device_free.
 */
HyScanNmeaUARTDevice *
hyscan_nmea_uart_device_copy (const HyScanNmeaUARTDevice *device)
{
  HyScanNmeaUARTDevice *new_device = g_slice_new (HyScanNmeaUARTDevice);

  new_device->name = g_strdup (device->name);
  new_device->path = g_strdup (device->path);

  return new_device;
}

/**
 * hyscan_nmea_uart_device_free:
 * @device: структура #HyScanNmeaUARTDevice для удаления
 *
 * Функция удаляет структуру #HyScanNmeaUARTDevice.
 */
void
hyscan_nmea_uart_device_free (HyScanNmeaUARTDevice *device)
{
  g_free ((gchar*)device->name);
  g_free ((gchar*)device->path);
  g_slice_free (HyScanNmeaUARTDevice, device);
}

G_DEFINE_BOXED_TYPE (HyScanNmeaUARTDevice, hyscan_nmea_uart_device, hyscan_nmea_uart_device_copy, hyscan_nmea_uart_device_free)
