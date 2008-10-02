/* $Id$ */
/* Global functions commonly used by source files

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

#ifndef __WC_GLOBAL_H__
#define __WC_GLOBAL_H__

#define xmlSafeFree(v)	if ((v) != NULL) xmlFree((v)); (v) = NULL;

enum retvalue
{
  RET_OK = 0,
  RET_EOF = 1,
  RET_ERROR = -1,
  RET_WARNING = -2
};

enum outputlvl
{
  LVL_ERR = -99,
  LVL_WARN = 0,
  LVL_NOTICE = 1,
  LVL_INFO = 2,
  LVL_DEBUG = 3
};

void outputf (int lvl, const char *fmt, ...);

#endif /* __WC_GLOBAL_H__ */
