/* $Id$ */
/* Everything dealing with a single monitor

   Copyright (C) 2006, 2007  Marius Konitzer
   This file is part of webchanges.

   webchanges is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   webchanges is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with webchanges; if not, write to the Free Software Foundation,
   Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA  */

#ifndef __WC_MONITOR_H__
#define __WC_MONITOR_H__

#include <libxml/xmlstring.h>
#include <libxml/xpath.h>
#include "vpair.h"

typedef struct _monitor monitor;
typedef monitor *monitorptr;

/* monitor functions */
monitorptr monitor_new (vpairptr vp, const xmlChar * name);
int monitor_evaluate (monitorptr m);
int monitor_triggered (const monitorptr m);
void monitor_free (monitorptr m);
int monitor_set_xpath (monitorptr m, const xmlChar * xpath);
int monitor_set_interval (monitorptr m, const xmlChar * ival);
int monitor_set_trigger (monitorptr m, const xmlChar * trigger);
const xmlChar *monitor_get_name (const monitorptr m);
xmlXPathObjectPtr monitor_get_old_result (const monitorptr m);
xmlXPathObjectPtr monitor_get_cur_result (const monitorptr m);
unsigned int monitor_get_interval (const monitorptr m);
vpairptr monitor_get_vpair (const monitorptr m);

#endif /* __WC_MONITOR_H__ */
