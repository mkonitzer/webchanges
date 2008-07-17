/* $Id$ */
/* Parse a given monitor file

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

#ifndef __WC_MONFILE_H__
#define __WC_MONFILE_H__

#include "monitor.h"
#include "vpair.h"

#define MONFILE_DTD_URL "http://webchanges.sourceforge.net/dtd/wc1.dtd"

typedef struct _monfile monfile;
typedef monfile *monfileptr;

/* monfile functions */
monfileptr monfile_open (const char *filename);
int monfile_get_next_vpair (const monfileptr mf, vpairptr * vp);
int monfile_get_next_monitor (const monfileptr mf, monitorptr * mon);
void monfile_close (monfileptr mf);
const xmlChar *monfile_get_filename (const monfileptr mf);
const xmlChar *monfile_get_name (const monfileptr mf);

#endif /* __WC_MONFILE_H__ */
