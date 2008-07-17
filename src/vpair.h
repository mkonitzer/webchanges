/* $Id$ */
/* Everything dealing with a version pair

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

#ifndef __WC_VPAIR_H__
#define __WC_VPAIR_H__

#include <libxml/xmlstring.h>

typedef struct _vpair vpair;
typedef vpair *vpairptr;

/* vpair functions */
vpairptr vpair_open (const xmlChar * url);
int vpair_parse (vpairptr vp);
int vpair_download (vpairptr vp);
int vpair_remove (vpairptr vp);
void vpair_close (vpairptr vp);
const xmlChar *vpair_get_url (const vpairptr vp);
const xmlChar *vpair_get_cache (const vpairptr vp);
const xmlDocPtr vpair_get_old_doc (const vpairptr vp);
const xmlDocPtr vpair_get_cur_doc (const vpairptr vp);

#endif /* __WC_VPAIR_H__ */
