/* $Id: metafile.h 257 2007-03-05 19:14:44Z marius $ */
#ifndef __WC_METAFILE_H__
#define __WC_METAFILE_H__

#include <time.h>
#include "monfile.h"
#include "monitor.h"

typedef struct _metafile metafile;
typedef metafile *metafileptr;

/* metafile functions */
metafileptr metafile_open (const monfileptr mf);
int metafile_read (metafileptr mef);
int metafile_write (metafileptr mef);
void metafile_close (metafileptr mef);

/* monitor functions */
int monitor_set_last_check (metafileptr mef, monitorptr m, time_t lastchk);
time_t monitor_get_last_check (metafileptr mef, monitorptr m);
time_t monitor_get_next_check (metafileptr mef, monitorptr m);

#endif /* __WC_METAFILE_H__ */
