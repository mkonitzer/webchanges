/* $Id$ */
/* Read and write metadata along with a monitor file

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
