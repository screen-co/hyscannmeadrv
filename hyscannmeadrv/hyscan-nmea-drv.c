/* hyscan-nmea-drv.c
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

#include "hyscan-nmea-drv.h"
#include "hyscan-nmea-discover.h"
#include <hyscan-data-schema-builder.h>
#include <hyscan-driver.h>

#include <glib/gi18n-lib.h>
#include <gmodule.h>

G_MODULE_EXPORT gpointer
hyscan_driver_discover (void)
{
  return hyscan_nmea_discover_new ();
}

G_MODULE_EXPORT gpointer
hyscan_driver_info (void)
{
  HyScanDataSchemaBuilder *builder;
  HyScanDataSchema *info;
  gchar *data;

  builder = hyscan_data_schema_builder_new ("driver-info");

  hyscan_data_schema_builder_key_integer_create (builder, "/schema/id",
                                                 "Schema id", NULL,
                                                 HYSCAN_DRIVER_SCHEMA_ID);
  hyscan_data_schema_builder_key_set_access     (builder, "/schema/id",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  hyscan_data_schema_builder_key_integer_create (builder, "/schema/version",
                                                 "Schema version", NULL,
                                                 HYSCAN_DRIVER_SCHEMA_VERSION);
  hyscan_data_schema_builder_key_set_access     (builder, "/schema/version",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  hyscan_data_schema_builder_key_string_create  (builder, "/info/name",
                                                 _ ("Name"), NULL,
                                                 "NMEA");
  hyscan_data_schema_builder_key_set_access     (builder, "/info/name",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  hyscan_data_schema_builder_key_string_create  (builder, "/info/description",
                                                 _ ("Description"), NULL,
                                                 _ ("HyScan NMEA driver"));
  hyscan_data_schema_builder_key_set_access     (builder, "/info/description",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  hyscan_data_schema_builder_key_string_create  (builder, "/info/version",
                                                 _ ("Version"), NULL,
                                                 HYSCAN_NMEA_DRIVER_VERSION);
  hyscan_data_schema_builder_key_set_access     (builder, "/info/version",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  hyscan_data_schema_builder_key_string_create  (builder, "/info/id",
                                                 _ ("Build id"), NULL,
                                                 HYSCAN_NMEA_DRIVER_BUILD_ID);
  hyscan_data_schema_builder_key_set_access     (builder, "/info/id",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  hyscan_data_schema_builder_key_integer_create (builder, "/api/version",
                                                 "API version", "API version",
                                                 HYSCAN_DISCOVER_API);
  hyscan_data_schema_builder_key_set_access     (builder, "/api/version",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  data = hyscan_data_schema_builder_get_data (builder);
  info = hyscan_data_schema_new_from_string (data, "driver-info");

  g_object_unref (builder);
  g_free (data);

  return info;
}
