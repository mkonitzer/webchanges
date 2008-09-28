/* $Id$ */
/* Everything dealing with the base directory

   Copyright (C) 2006, 2007, 2008  Marius Konitzer
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

#ifndef __WC_BASEDIR_H__
#define __WC_BASEDIR_H__

typedef struct _basedir basedir;
typedef basedir *basedirptr;

basedirptr basedir_open (const char *dirname);
char *basedir_get_monfile (const basedirptr bd, const char *fn);
char *basedir_get_metafile (const basedirptr bd, const char *fn);
char *basedir_get_cache (const basedirptr bd, const char *fn);
int basedir_usable (const basedirptr bd);
void basedir_safefree (char *filename);

#endif /* __WC_BASEDIR_H__ */
