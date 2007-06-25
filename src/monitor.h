/* $Id$ */
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
int monitor_triggered (monitorptr m);
void monitor_free (monitorptr m);
int monitor_set_xpath (monitorptr m, const xmlChar * xpath);
int monitor_set_interval (monitorptr m, const xmlChar * ival);
int monitor_set_trigger (monitorptr m, const xmlChar * trigger);
xmlChar *monitor_get_name (monitorptr m);
xmlXPathObjectPtr monitor_get_old_result (monitorptr m);
xmlXPathObjectPtr monitor_get_cur_result (monitorptr m);
unsigned int monitor_get_interval (monitorptr m);
vpairptr monitor_get_vpair (monitorptr m);

#endif /* __WC_MONITOR_H__ */
