/* hyscan-nmea-receiver.c
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
 * SECTION: hyscan-nmea-receiver
 * @Short_description: класс приёма NMEA данных
 * @Title: HyScanNmeaReceiver
 *
 * Класс предназначен для приёма NMEA данных. Во время приёма данных
 * производится автоматическая группировка NMEA строк в блоки, по времени
 * принятия решения навигационной ситсемой. Для этих целей используются
 * строки GGA, RMC, GLL, BWC и ZDA. Одновременно с этим производится фиксация
 * момента времени прихода первого символа первой строки блока.
 *
 * Объект HyScanNmeaReceiver создаётся с помощью функции
 * #hyscan_nmea_receiver_new.
 *
 * Если какая-либо из NMEA строк принята с ошибкой, дальнейшая обработка этой
 * строки определяется с помощью функции #hyscan_nmea_receiver_skip_broken.
 *
 * Приём данных от GPS устройства или других источников должен быть реализован
 * сторонними классами. HyScanNmeaReceiver обрабатывает уже принятые данные.
 * Для передачи данных предназначена функция #hyscan_nmea_receiver_add_data.
 *
 * Блок данных отправляется пользователю в момент изменения времени в любой
 * из NMEA строк. В обычной ситуации это приводит к задержке отправки данных
 * пользователю на один цикл приёма (на определение времени приёма это не
 * влияет). Если пользователю необходимо получать данные более оперативно, он
 * может использовать функцию #hyscan_nmea_receiver_flush для отправки текущего
 * блока данных, если в течение времени timeout не было новых данных.
 * Отправка готовых блоков данных пользователю осуществляется через сигнал
 * #HyScanNmeaReceiver::nmea-data.
 *
 * Функция #hyscan_nmea_receiver_send_log может использоваться для отправки
 * информационных сообщений.
 *
 * Если была обнаружена ошибка ввода/вывода, можно использовать функцию
 * #hyscan_nmea_receiver_io_error для сигнализирования о ней.
 */

#include "hyscan-nmea-receiver.h"
#include "hyscan-nmea-marshallers.h"

#include <hyscan-slice-pool.h>

#include <string.h>
#include <stdio.h>

#define N_BUFFERS 16
#define MAX_MSG_SIZE 4084
#define MAX_STRING_SIZE 253
#define RX_TIMEOUT 2.0

enum
{
  SIGNAL_NMEA_DATA,
  SIGNAL_NMEA_LOG,
  SIGNAL_NMEA_IO_ERROR,
  SIGNAL_LAST
};

typedef struct
{
  gint64           time;                       /* Время приёма сообщения. */
  gchar            data[MAX_MSG_SIZE];         /* Данные. */
  guint32          size;                       /* Размер сообщения. */
} HyScanNmeaReceiverMessage;

struct _HyScanNmeaReceiverPrivate
{
  GThread         *emmiter;                    /* Поток отправки данных. */

  gboolean         terminate;                  /* Признак необходимости завершения работы. */
  gboolean         skip_broken;                /* Признак необходимости пропуска битых NMEA строк. */

  GTimer          *timeout;                    /* Таймер отправки сообщения по таймауту. */
  GAsyncQueue     *queue;                      /* Очередь сообщений для отправки клиенту. */
  HyScanSlicePool *buffers;                    /* Список буферов приёма данных. */
  GRWLock          lock;                       /* Блокировка доступа к списку буферов. */

  gint64           rx_time;                    /* Метка времени приёма начала строки. */
  gint             nmea_time;                  /* NMEA время сообщения. */
  gint64           message_time;               /* Метка времени сообщения. */

  gchar            message[MAX_MSG_SIZE];      /* Буфер собираемого сообщения. */
  guint32          message_size;               /* Размер сообщения. */

  gchar            string[MAX_STRING_SIZE+3];  /* NMEA строка. */
  guint            string_size;                /* Размер NMEA строки. */
};

static void        hyscan_nmea_receiver_object_constructed (GObject       *object);
static void        hyscan_nmea_receiver_object_finalize    (GObject       *object);

static gpointer    hyscan_nmea_receiver_emmiter            (gpointer       user_data);

static guint       hyscan_nmea_receiver_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanNmeaReceiver, hyscan_nmea_receiver, G_TYPE_OBJECT)

static void
hyscan_nmea_receiver_class_init (HyScanNmeaReceiverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_nmea_receiver_object_constructed;
  object_class->finalize = hyscan_nmea_receiver_object_finalize;

  /**
   * HyScanNmeaReceiver::nmea-data:
   * @receiver: указатель на #HyScanNmeaReceiver
   * @time: метка времени приёма данных, мкс
   * @data: NMEA данные
   * @size: размер NMEA данных
   *
   * Данный сигнал посылается при получении от устройства блока NMEA данных.
   * Данные представлены в виде NULL терминированой строки. Размер включает
   * в себя нулевой символ.
   */
  hyscan_nmea_receiver_signals[SIGNAL_NMEA_DATA] =
    g_signal_new ("nmea-data", HYSCAN_TYPE_NMEA_RECEIVER, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  hyscan_nmea_marshal_VOID__INT64_STRING_UINT,
                  G_TYPE_NONE,
                  3, G_TYPE_INT64, G_TYPE_STRING, G_TYPE_UINT);

  /**
   * HyScanNmeaReceiver::nmea-log:
   * @receiver: указатель на #HyScanNmeaReceiver
   * @time: метка времени сообщения, мкс
   * @level: тип сообщения #HyScanLogLevel
   * @message: сообщение (NULL терминированная строка)
   *
   * В процессе работы драйвер может формировать различные информационные и
   * диагностические сообщения. Этот сигнал предназначен для информирования
   * драйвера о состоянии NMEA приёмника.
   */
  hyscan_nmea_receiver_signals[SIGNAL_NMEA_LOG] =
    g_signal_new ("nmea-log", HYSCAN_TYPE_NMEA_RECEIVER, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  hyscan_nmea_marshal_VOID__INT64_INT_STRING,
                  G_TYPE_NONE, 3, G_TYPE_INT64, G_TYPE_INT, G_TYPE_STRING);

  /**
   * HyScanNmeaReceiver::nmea-io-error:
   * @receiver: указатель на #HyScanNmeaReceiver
   *
   * Данный сигнал посылается если на уровне приёма данных обнаружена ошибка и
   * дальнейший приём невозможен. Это может произойти, например, если был
   * отключен USB-RS или USB-Ethernet конвертер.
   */
  hyscan_nmea_receiver_signals[SIGNAL_NMEA_IO_ERROR] =
    g_signal_new ("nmea-io-error", HYSCAN_TYPE_NMEA_RECEIVER, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_nmea_receiver_init (HyScanNmeaReceiver *receiver)
{
  receiver->priv = hyscan_nmea_receiver_get_instance_private (receiver);
}

static void
hyscan_nmea_receiver_object_constructed (GObject *object)
{
  HyScanNmeaReceiver *receiver = HYSCAN_NMEA_RECEIVER (object);
  HyScanNmeaReceiverPrivate *priv = receiver->priv;
  guint i;

  g_rw_lock_init (&priv->lock);

  priv->timeout = g_timer_new ();

  priv->queue = g_async_queue_new_full (g_free);
  for (i = 0; i < N_BUFFERS; i++)
    hyscan_slice_pool_push (&priv->buffers, g_new (HyScanNmeaReceiverMessage, 1));

  priv->emmiter = g_thread_new ("nmea-emmiter", hyscan_nmea_receiver_emmiter, receiver);
}

static void
hyscan_nmea_receiver_object_finalize (GObject *object)
{
  HyScanNmeaReceiver *receiver = HYSCAN_NMEA_RECEIVER (object);
  HyScanNmeaReceiverPrivate *priv = receiver->priv;
  gpointer buffer;

  g_atomic_int_set (&priv->terminate, TRUE);
  g_thread_join (priv->emmiter);

  g_async_queue_unref (priv->queue);
  while ((buffer = hyscan_slice_pool_pop (&priv->buffers)) != NULL)
    g_free (buffer);

  g_timer_destroy (priv->timeout);

  g_rw_lock_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_nmea_receiver_parent_class)->finalize (object);
}

/* Поток отправки данных клиенту. */
static gpointer
hyscan_nmea_receiver_emmiter (gpointer user_data)
{
  HyScanNmeaReceiver *receiver = user_data;
  HyScanNmeaReceiverPrivate *priv = receiver->priv;
  HyScanNmeaReceiverMessage *message;

  while (!g_atomic_int_get (&priv->terminate))
    {
      message = g_async_queue_timeout_pop (priv->queue, 100000);
      if (message == NULL)
        continue;

      g_signal_emit (receiver, hyscan_nmea_receiver_signals[SIGNAL_NMEA_DATA], 0,
                     message->time, message->data, message->size);

      g_rw_lock_writer_lock (&priv->lock);
      hyscan_slice_pool_push (&priv->buffers, message);
      g_rw_lock_writer_unlock (&priv->lock);
    }

  return NULL;
}

/**
 * hyscan_nmea_receiver_new:
 *
 * Функция создаёт новый объект #HyScanNmeaReceiver.
 *
 * Returns: #HyScanNmeaReceiver. Для удаления #g_object_unref.
 */
HyScanNmeaReceiver *
hyscan_nmea_receiver_new (void)
{
  return g_object_new (HYSCAN_TYPE_NMEA_RECEIVER, NULL);
}

/**
 * hyscan_nmea_receiver_skip_broken:
 * @receiver: указатель на #HyScanNmeaReceiver
 * @skip: признак пропуска некорректных NMEA строк
 *
 * Функция задаёт поведение при приёме некорректных NMEA строк. Некорректной
 * является строка у которой не совпадает контрольная сумма.
 */
void
hyscan_nmea_receiver_skip_broken (HyScanNmeaReceiver *receiver,
                                  gboolean            skip)
{
  g_return_if_fail (HYSCAN_IS_NMEA_RECEIVER (receiver));

  g_atomic_int_set (&receiver->priv->skip_broken, skip);
}

/**
 * hyscan_nmea_receiver_add_data:
 * @receiver: указатель на #HyScanNmeaReceiver
 * @time: метка времени приёма данных
 * @data: принятые данные
 * @size: размер данных
 *
 * Функция обрабатывает принятые данные.
 *
 * Returns: %TRUE если по результатам обработки обнаружена валидная
 * NMEA строка, иначе %FALSE.
 */
gboolean
hyscan_nmea_receiver_add_data (HyScanNmeaReceiver *receiver,
                               gint64              time,
                               const gchar        *data,
                               guint32             size)
{
  HyScanNmeaReceiverPrivate *priv;
  gboolean good_nmea = FALSE;
  guint32 rxi;

  g_return_val_if_fail (HYSCAN_IS_NMEA_RECEIVER (receiver), FALSE);

  priv = receiver->priv;

  /* Если данные не приходили длительное время, очистим текущий буфер. */
  if (g_timer_elapsed (priv->timeout, NULL) > RX_TIMEOUT)
    {
      priv->message_time = 0;
      priv->message_size = 0;
      priv->string_size = 0;
    }

  /* Обрабатываем данные по отдельным символам. */
  for (rxi = 0; rxi < size; rxi++)
    {
      gchar rx_data = data[rxi];

      /* Время приёма начала строки. */
      if (rx_data == '$')
        priv->rx_time = time;

      /* Фиксируем время начала приёма блока. */
      if (priv->message_time == 0)
        priv->message_time = priv->rx_time;

      /* Текущая обрабатываемая строка пустая и данные не являются началом строки. */
      if ((priv->string_size == 0) && (rx_data != '$'))
        continue;

      /* Собираем строку до тех пор пока не встретится символ '\r'. */
      if (rx_data != '\r')
        {
          /* Если строка слишком длинная, пропускаем её. */
          if (priv->string_size > MAX_STRING_SIZE)
            {
              priv->string_size = 0;
              continue;
            }

          /* Сохраняем текущий символ. */
          priv->string [priv->string_size++] = rx_data;
          priv->string [priv->string_size] = 0;
          continue;
        }

      /* Строка собрана. */
      else
        {
          gboolean send_block = FALSE;
          gboolean bad_crc = FALSE;
          guchar nmea_crc1 = 0;
          guint nmea_crc2 = 255;
          guint i;

          /* NMEA строка не может быть короче 10 символов. */
          if (priv->string_size < 10)
            {
              priv->string_size = 0;
              continue;
            }

          /* Проверяем контрольную сумму NMEA строки. */
          priv->string[priv->string_size] = 0;
          for (i = 1; i < priv->string_size - 3; i++)
            nmea_crc1 ^= priv->string[i];

          /* Если контрольная сумма не совпадает, не используем время из это строки. */
          if ((sscanf (priv->string + priv->string_size - 3, "*%02X", &nmea_crc2) != 1) ||
              (nmea_crc1 != nmea_crc2))
            {
              bad_crc = TRUE;
            }

          /* Пропускаем "плохие" NMEA строки. */
          if (g_atomic_int_get (&priv->skip_broken) && bad_crc)
            {
              priv->string_size = 0;
              continue;
            }

          /* Признак корректной NMEA строки. */
          good_nmea = TRUE;

          /* Вытаскиваем время из стандартных NMEA строк. */
          if ((g_str_has_prefix (priv->string + 3, "GGA") ||
               g_str_has_prefix (priv->string + 3, "RMC") ||
               g_str_has_prefix (priv->string + 3, "BWC") ||
               g_str_has_prefix (priv->string + 3, "ZDA")) && !bad_crc)
            {
              gint nmea_time, hour, min, sec, msec;
              gint n_fields;

              /* Смещение до поля со временем во всех этих строках равно 7. */
              n_fields = sscanf (priv->string + 7,"%2d%2d%2d.%d", &hour, &min, &sec, &msec);
              if (n_fields == 3)
                nmea_time = 1000 * (3600 * hour + 60 * min + sec);
              else if (n_fields == 4)
                nmea_time = 1000 * (3600 * hour + 60 * min + sec) + msec;
              else
                nmea_time = 0;

              /* Если текущее время и время блока различаются, отправляем блок данных. */
              if ((priv->nmea_time > 0) && (priv->nmea_time != nmea_time))
                send_block = TRUE;

              priv->nmea_time = nmea_time;
            }

          /* Если в блоке больше нет места, отправляем блок. */
          if ((priv->message_size + priv->string_size + 3) > MAX_MSG_SIZE)
            send_block = TRUE;

          /* Если нет возможности определить время из строки,
           * отправляем строку без объединения в блок. */
          if (priv->nmea_time == 0)
            {
              HyScanNmeaReceiverMessage *message;

              priv->string[priv->string_size++] = '\r';
              priv->string[priv->string_size++] = '\n';
              priv->string[priv->string_size++] = 0;

              g_rw_lock_writer_lock (&priv->lock);
              message = hyscan_slice_pool_pop (&priv->buffers);
              g_rw_lock_writer_unlock (&priv->lock);

              if (message != NULL)
                {
                  message->time = priv->rx_time;
                  message->size = priv->string_size;
                  memcpy (message->data, priv->string, priv->string_size);
                  g_async_queue_push (priv->queue, message);
                }

              priv->message_time = 0;
              priv->message_size = 0;
              priv->string_size = 0;
              continue;
            }

          /* Отправляем блок данных. */
          if (send_block && (priv->message_size > 0))
            {
              HyScanNmeaReceiverMessage *message;

              g_rw_lock_writer_lock (&priv->lock);
              message = hyscan_slice_pool_pop (&priv->buffers);
              g_rw_lock_writer_unlock (&priv->lock);

              if (message != NULL)
                {
                  message->time = priv->message_time;
                  message->size = priv->message_size + 1;
                  memcpy (message->data, priv->message, priv->message_size + 1);
                  g_async_queue_push (priv->queue, message);
                }

              priv->message_time = 0;
              priv->message_size = 0;
            }

          /* Сохраняем строку в блоке. */
          memcpy (priv->message + priv->message_size, priv->string, priv->string_size);
          priv->message_size += priv->string_size;
          priv->message [priv->message_size++] = '\r';
          priv->message [priv->message_size++] = '\n';
          priv->message [priv->message_size] = 0;

          priv->string_size = 0;
        }
    }

  if (size > 0)
    g_timer_start (priv->timeout);

  return good_nmea;
}

/**
 * hyscan_nmea_receiver_flush:
 * @receiver: указатель на #HyScanNmeaReceiver
 * @timeout: таймаут отправки данных
 *
 * Функция отправляет блок NMEA данных по таймауту.
 */
void
hyscan_nmea_receiver_flush (HyScanNmeaReceiver  *receiver,
                            gdouble              timeout)
{
  HyScanNmeaReceiverPrivate *priv;

  g_return_if_fail (HYSCAN_IS_NMEA_RECEIVER (receiver));

  priv = receiver->priv;

  if ((g_timer_elapsed (priv->timeout, NULL) > timeout) && (priv->message_size > 0))
    {
      HyScanNmeaReceiverMessage *message;

      g_rw_lock_writer_lock (&priv->lock);
      message = hyscan_slice_pool_pop (&priv->buffers);
      g_rw_lock_writer_unlock (&priv->lock);

      if (message != NULL)
        {
          message->time = priv->message_time;
          message->size = priv->message_size + 1;
          memcpy (message->data, priv->message, priv->message_size + 1);
          g_async_queue_push (priv->queue, message);

          priv->message_time = 0;
          priv->message_size = 0;
        }

      g_timer_start (priv->timeout);
    }
}

/**
 * hyscan_nmea_receiver_io_error:
 * @receiver: указатель на #HyScanNmeaReceiver
 * @time: метка времени сообщения, мкс
 * @level: тип сообщения
 * @message: сообщение
 *
 * Функция отправляет сигнал #HyScanNmeaReceiver::nmea-log.
 */
void
hyscan_nmea_receiver_send_log (HyScanNmeaReceiver *receiver,
                               gint64              time,
                               HyScanLogLevel      level,
                               const gchar        *message)
{
  g_signal_emit (receiver, hyscan_nmea_receiver_signals[SIGNAL_NMEA_IO_ERROR], 0,
                 time, level, message);

}

/**
 * hyscan_nmea_receiver_io_error:
 * @receiver: указатель на #HyScanNmeaReceiver
 *
 * Функция отправляет сигнал #HyScanNmeaReceiver::nmea-io-error.
 */
void
hyscan_nmea_receiver_io_error (HyScanNmeaReceiver *receiver)
{
  g_signal_emit (receiver, hyscan_nmea_receiver_signals[SIGNAL_NMEA_IO_ERROR], 0);
}
