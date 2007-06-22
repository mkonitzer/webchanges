/* $Id: monfile.h 259 2007-03-06 02:18:18Z marius $ */
#ifndef __WC_MONFILE_H__
#define __WC_MONFILE_H__

#include "monitor.h"
#include "vpair.h"

#define MONFILE_DTD_URL "http://webchanges.sourceforge.net/dtd/wc1.dtd"

typedef struct _monfile monfile;
typedef monfile *monfileptr;

/* monfile functions */
monfileptr monfile_open (const char *filename);
int monfile_get_next_vpair (monfileptr mf, vpairptr * vp);
int monfile_get_next_monitor (monfileptr mf, monitorptr * mon);
void monfile_close (monfileptr mf);
xmlChar *monfile_get_filename (monfileptr mf);
xmlChar *monfile_get_name (monfileptr mf);

#endif /* __WC_MONFILE_H__ */
