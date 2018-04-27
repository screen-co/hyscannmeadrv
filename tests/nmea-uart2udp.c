#include <hyscan-nmea-uart.h>
#include <gio/gio.h>

#include <string.h>
#include <stdio.h>

void
data_cb (HyScanNmeaUART *uart,
         gint64          time,
         const gchar    *nmea,
         guint           size,
         gpointer        user_data)
{
  GSocket *socket = user_data;
  gdouble dtime = time / 1000000.0;
  static gchar buffer[128] = {0};
  static guint buffer_used = 0;
  guint send_size = sizeof (buffer) - 1;
  guint index = 0;

  /* Отправляем данные блоками по send_size байт. */
  while (size > send_size)
    {
      guint copy_size = send_size - buffer_used;

      memcpy (buffer + buffer_used, nmea + index, copy_size);
      size -= copy_size;
      index += copy_size;

      g_socket_send (socket, buffer, send_size, NULL, NULL);
      buffer_used = 0;
    }

  /* Сохраняем остатки данных для следующей отправки. */
  memcpy (buffer, nmea + index, size);
  buffer_used = size;

  g_print ("UART: rx time %.03fs\n%s\n", dtime, nmea);
}

int
main (int    argc,
      char **argv)
{
  GSocket *socket;
  GSocketAddress *address;
  HyScanNmeaUART *uart;

  gchar *uart_path = NULL;
  gchar *host = NULL;
  gint port = 0;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "uart", 'u', 0, G_OPTION_ARG_STRING, &uart_path, "Path to uart device", NULL },
        { "host", 'h', 0, G_OPTION_ARG_STRING, &host, "Destination ip address", NULL },
        { "port", 'p', 0, G_OPTION_ARG_INT, &port, "Destination udp port", NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if ((uart_path == NULL) || (host == NULL) || (port < 1024) || (port > 65535))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
    g_strfreev (args);
  }

  address = g_inet_socket_address_new_from_string (host, port);
  if (address == NULL)
    g_error ("unknown host address");

  socket = g_socket_new (g_socket_address_get_family (address),
                         G_SOCKET_TYPE_DATAGRAM,
                         G_SOCKET_PROTOCOL_DEFAULT,
                         NULL);
  if (socket == NULL)
    g_error ("can't create socket");

  if (!g_socket_connect (socket, address, NULL, NULL))
    g_error ("can't connect to %s:%d", host, port);

  uart = hyscan_nmea_uart_new ();
  hyscan_nmea_uart_set_device (uart, uart_path, HYSCAN_NMEA_UART_MODE_AUTO);
  g_signal_connect (uart, "nmea-data", G_CALLBACK (data_cb), socket);

  g_message ("Press [Enter] to terminate test...");
  getchar ();

  g_object_unref (uart);
  g_object_unref (socket);
  g_object_unref (address);

  return 0;
}
