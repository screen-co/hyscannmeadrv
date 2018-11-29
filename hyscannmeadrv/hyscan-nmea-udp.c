/* hyscan-nmea-udp.c
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

/**
 * SECTION: hyscan-nmea-udp
 * @Short_description: класс приёма NMEA данных через UDP/IP порты
 * @Title: HyScanNmeaUDP
 *
 * Класс предназначен для приёма NMEA данных через UDP/IP порты. Класс
 * наследуется от #HyScanNmeaReceiver.
 *
 * Объект HyScanNmeaUDP создаётся с помощию функции #hyscan_nmea_udp_new.
 * IP адрес и UDP порт для приёма данных задаются с помощью функции
 * #hyscan_nmea_udp_set_address.
 *
 * Список IP адресов доступных в системе можно узнать с помощью функции
 * #hyscan_nmea_udp_list_addresses.
 */

#include "hyscan-nmea-udp.h"
#include "hyscan-nmea-marshallers.h"

#include <gio/gnetworking.h>
#include <gio/gio.h>

#ifdef G_OS_UNIX
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif

#ifdef G_OS_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif

#define N_BUFFERS      64

struct _HyScanNmeaUDPPrivate
{
  GThread             *receiver;       /* Поток приёма данных. */

  gboolean             started;        /* Признак работы потока приёма данных. */
  gboolean             configure;      /* Признак режима конфигурации UART порта. */
  gboolean             terminate;      /* Признак необходимости завершения работы. */

  GSocket             *socket;         /* Сокет для приёма данных по UDP. */
};

static void            hyscan_nmea_udp_object_constructed      (GObject               *object);
static void            hyscan_nmea_udp_object_finalize         (GObject               *object);

static gpointer        hyscan_nmea_udp_receiver                (gpointer               user_data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanNmeaUDP, hyscan_nmea_udp, HYSCAN_TYPE_NMEA_RECEIVER)

static void
hyscan_nmea_udp_class_init (HyScanNmeaUDPClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_nmea_udp_object_constructed;
  object_class->finalize = hyscan_nmea_udp_object_finalize;
}

static void
hyscan_nmea_udp_init (HyScanNmeaUDP *udp)
{
  udp->priv = hyscan_nmea_udp_get_instance_private (udp);
}

static void
hyscan_nmea_udp_object_constructed (GObject *object)
{
  HyScanNmeaUDP *udp = HYSCAN_NMEA_UDP (object);
  HyScanNmeaUDPPrivate *priv = udp->priv;

  G_OBJECT_CLASS (hyscan_nmea_udp_parent_class)->constructed (object);

  priv->started = TRUE;

  priv->receiver = g_thread_new ("udp-receiver", hyscan_nmea_udp_receiver, udp);
}

static void
hyscan_nmea_udp_object_finalize (GObject *object)
{
  HyScanNmeaUDP *udp = HYSCAN_NMEA_UDP (object);
  HyScanNmeaUDPPrivate *priv = udp->priv;

  g_atomic_int_set (&priv->terminate, TRUE);
  g_thread_join (priv->receiver);

  g_clear_object (&priv->socket);

  G_OBJECT_CLASS (hyscan_nmea_udp_parent_class)->finalize (object);
}

/* Поток приёма данных. */
static gpointer
hyscan_nmea_udp_receiver (gpointer user_data)
{
  HyScanNmeaUDP *udp = user_data;
  HyScanNmeaReceiver *nmea = user_data;
  HyScanNmeaUDPPrivate *priv = udp->priv;

  gchar rx_data[65536];
  gssize rx_size;
  gint64 rx_time;

  while (!g_atomic_int_get (&priv->terminate))
    {
      /* Режим конфигурации. */
      if (g_atomic_int_get (&priv->configure))
        {
          g_clear_object (&priv->socket);

          /* Ждём завершения конфигурации. */
          g_atomic_int_set (&priv->started, FALSE);
          g_usleep (100000);
          continue;
        }

      /* Конфигурация завершена. */
      else
        {
          /* Адрес не установлен. */
          if (priv->socket == NULL)
            {
              g_usleep (100000);
              continue;
            }

          /* Ожидаем данные. */
          if (!g_socket_condition_timed_wait (priv->socket, G_IO_IN, 100000, NULL, NULL))
            continue;

          /* Время приёма данных. */
          rx_time = g_get_monotonic_time ();

          /* Приём данных и обработка. */
          rx_size = g_socket_receive (priv->socket, rx_data, sizeof (rx_data) - 1, NULL, NULL);
          if (rx_size > 0)
            hyscan_nmea_receiver_add_data (nmea, rx_time, rx_data, rx_size);
        }
    }

  return NULL;
}

/**
 * hyscan_nmea_udp_new:
 *
 * Функция создаёт новый объект #HyScanNmeaUDP.
 *
 * Returns: #HyScanNmeaUDP. Для удаления #g_object_unref.
 */
HyScanNmeaUDP *
hyscan_nmea_udp_new (void)
{
  return g_object_new (HYSCAN_TYPE_NMEA_UDP, NULL);
}

/**
 * hyscan_nmea_udp_set_address:
 * @udp: указатель на #HyScanNmeaUDP
 * @ip: IP адрес
 * @port: UDP порт
 *
 * Функция устанавливает IP адрес и номер UDP порта для приёма данных.
 * В качестве IP адреса могут быть переданы специальные названия "any"
 * и "loopback", которые используются для выбора всех IP v4 адресов и
 * loopback адреса соответственно.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_nmea_udp_set_address (HyScanNmeaUDP *udp,
                             const gchar   *ip,
                             guint16        port)
{
  HyScanNmeaUDPPrivate *priv;

  GSocketAddress *address = NULL;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_NMEA_UDP (udp), FALSE);

  priv = udp->priv;

  /* Переходим в режим конфигурации. */
  while (!g_atomic_int_compare_and_exchange (&priv->configure, FALSE, TRUE))
    g_usleep (10000);
  while (g_atomic_int_get (&priv->started))
    g_usleep (10000);

  /* Устройство отключено. */
  if (ip == NULL || port < 1024)
    {
      status = TRUE;
      goto exit;
    }

  /* Закрываем предыдущий сокет приёма данных. */
  g_clear_object (&priv->socket);

  /* Адрес подключения. */
  if (g_strcmp0 (ip, "any") == 0)
    {
      GInetAddress *inet_addr = g_inet_address_new_any (G_SOCKET_FAMILY_IPV4);
      address = g_inet_socket_address_new (inet_addr, port);
      g_object_unref (inet_addr);
    }
  else if (g_strcmp0 (ip, "loopback") == 0)
    {
      GInetAddress *inet_addr = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
      address = g_inet_socket_address_new (inet_addr, port);
      g_object_unref (inet_addr);
    }
  else
    {
      address = g_inet_socket_address_new_from_string (ip, port);
    }

  if (address == NULL)
    goto exit;

  /* Сокет приёма данных. */
  priv->socket = g_socket_new (g_socket_address_get_family (address),
                               G_SOCKET_TYPE_DATAGRAM,
                               G_SOCKET_PROTOCOL_DEFAULT,
                               NULL);
  if (priv->socket == NULL)
    goto exit;

  /* Размер приёмного буфера. */
  status = g_socket_set_option (priv->socket, SOL_SOCKET, SO_RCVBUF, N_BUFFERS * 4096, NULL);
  if (!status)
    {
      g_clear_object (&priv->socket);
      goto exit;
    }

  /* Привязка к рабочему адресу и порту. */
  status = g_socket_bind (priv->socket, address, FALSE, NULL);
  if (!status)
    g_clear_object (&priv->socket);

exit:
  g_clear_object (&address);

  /* Завершаем конфигурацию. */
  g_atomic_int_set (&priv->started, TRUE);
  g_atomic_int_set (&priv->configure, FALSE);

  return status;
}

/**
 * hyscan_nmea_udp_list_addresses:
 *
 * Функция возвращает список IP адресов доступных в системе.
 *
 * Returns: (transfer full): Список IP адресов или NULL.
 * Для удаления #g_strfreev.
 */
gchar **
hyscan_nmea_udp_list_addresses (void)
{
#ifdef G_OS_UNIX
  struct ifaddrs *ifap, *ifa;
  GPtrArray *list;

  if (getifaddrs (&ifap) != 0)
    return NULL;

  list = g_ptr_array_new ();
  for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next)
    {
      if (ifa->ifa_addr == NULL)
        continue;

      if (ifa->ifa_addr->sa_family == AF_INET)
        {
          struct sockaddr_in *sa;
          sa = (struct sockaddr_in *) ifa->ifa_addr;
          g_ptr_array_add (list, g_strdup (inet_ntoa (sa->sin_addr)));
        }
    }
  freeifaddrs(ifap);

  if (list->len > 0)
    {
      gchar **addresses;

      g_ptr_array_add (list, NULL);
      addresses = (gpointer)list->pdata;
      g_ptr_array_free (list, FALSE);

      return addresses;
    }

  return NULL;
#endif

#ifdef G_OS_WIN32
  static gboolean network_init = FALSE;

  DWORD ret, paa_size;
  PIP_ADAPTER_ADDRESSES paa, caa;
  GPtrArray *list;

  if (g_atomic_int_compare_and_exchange (&network_init, FALSE, TRUE))
    {
      WSADATA wsa_data;
      WSAStartup (MAKEWORD (2, 0), &wsa_data);
    }

  paa_size = 65536;
  paa = g_malloc (paa_size);

  do
    {
      if (paa == NULL)
        paa = g_malloc (paa_size);

      ret = GetAdaptersAddresses (AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, paa, &paa_size);

      if (ret == ERROR_BUFFER_OVERFLOW)
        g_clear_pointer (&paa, g_free);
    }
  while (ret == ERROR_BUFFER_OVERFLOW);

  if (ret != NO_ERROR)
    return NULL;

  list = g_ptr_array_new ();
  for (caa = paa; caa != NULL; caa = caa->Next)
    {
      PIP_ADAPTER_UNICAST_ADDRESS ua;

      for (ua = caa->FirstUnicastAddress; ua != NULL; ua = ua->Next)
        {
          gchar address [BUFSIZ] = {0};

          if (getnameinfo (ua->Address.lpSockaddr, ua->Address.iSockaddrLength,
                           address, BUFSIZ, NULL, 0, NI_NUMERICHOST) == 0)
            {
              g_ptr_array_add (list, g_strdup (address));
            }
        }
    }
  g_free (paa);

  if (list->len > 0)
    {
      gchar **addresses;

      g_ptr_array_add (list, NULL);
      addresses = (gpointer)list->pdata;
      g_ptr_array_free (list, FALSE);

      return addresses;
    }

  g_ptr_array_free (list, TRUE);

  return NULL;
#endif
}
