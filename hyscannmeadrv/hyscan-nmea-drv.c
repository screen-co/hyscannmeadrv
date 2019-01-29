/* hyscan-nmea-drv.c
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

#include "hyscan-nmea-drv.h"
#include "hyscan-nmea-discover.h"
#include <hyscan-driver-schema.h>

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
  HyScanDriverSchema *driver;
  HyScanDataSchema *info;

  driver = hyscan_driver_schema_new (HYSCAN_DRIVER_SCHEMA_VERSION);
  builder = HYSCAN_DATA_SCHEMA_BUILDER (driver);

  hyscan_data_schema_builder_key_string_create  (builder, "/info/name",
                                                 _("Name"), NULL,
                                                 "NMEA-0183");
  hyscan_data_schema_builder_key_set_access     (builder, "/info/name",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READ);

  hyscan_data_schema_builder_key_string_create  (builder, "/info/version",
                                                 _("Version"), NULL,
                                                 HYSCAN_NMEA_DRIVER_VERSION);
  hyscan_data_schema_builder_key_set_access     (builder, "/info/version",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READ);

  hyscan_data_schema_builder_key_string_create  (builder, "/info/id",
                                                 _("Build id"), NULL,
                                                 HYSCAN_NMEA_DRIVER_BUILD_ID);
  hyscan_data_schema_builder_key_set_access     (builder, "/info/id",
                                                 HYSCAN_DATA_SCHEMA_ACCESS_READ);

  info = hyscan_data_schema_builder_get_schema (builder);

  g_object_unref (builder);

  return info;
}
