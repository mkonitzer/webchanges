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

#include <libxml/xmlstring.h>
#include <libxml/hash.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "metafile.h"
#include "monitor.h"
#include "basedir.h"
#include "global.h"

struct _metafile
{
  /* user-filled variables */
  monfileptr mf;
  /* state variables */
  char *filename;
  xmlHashTablePtr monitors;
};

typedef struct
{
  int seen;
  time_t lastchk;
} monmeta;
typedef monmeta *monmetaptr;

static char *
monfile_to_metafile (const char *filename)
{
  char *ret = NULL;
  int len = strlen (filename);
  if (strncasecmp (filename + len - 4, ".xml", 4) == 0)
    {
      ret = (char *) malloc (len + 2);
      strncpy (ret, filename, len - 4);
      ret[len - 4] = '\0';
    }
  else
    {
      ret = (char *) malloc (len + 6);
      strcpy (ret, filename);
    }
  return strcat (ret, ".meta");
}

metafileptr
metafile_open (const monfileptr mf)
{
  char *filename = NULL;
  basedirptr bd = NULL;
  metafileptr mef;
  /* allocate metafile struct */
  mef = (metafileptr) xmlMalloc (sizeof (metafile));
  if (mef == NULL)
    {
      outputf (LVL_ERR, "[metafile] Out of memory\n");
      return NULL;
    }
  /* fill metafile struct */
  memset (mef, 0, sizeof (metafile));
  mef->mf = mf;
  /* calculate meta filename */
  bd = monfile_get_basedir (mf);
  filename = monfile_to_metafile (monfile_get_filename (mf));
  mef->filename = basedir_buildpath_metafile (bd, filename);
  free (filename);
  outputf (LVL_DEBUG, "[metafile] Using metadata file %s\n", mef->filename);
  return mef;
}

int
metafile_read (metafileptr mef)
{
  FILE *f;
  time_t chk = 0;
  xmlChar buf[31], *name = buf;
  if (mef->monitors != NULL)
    xmlHashFree (mef->monitors, (xmlHashDeallocator) xmlFree);
  mef->monitors = xmlHashCreate (0);
  f = fopen (mef->filename, "r");
  if (f == NULL)
    {
      outputf (LVL_ERR, "[metafile] Could not open %s for reading\n",
	       mef->filename);
      return RET_ERROR;
    }
  /* read metadata file line-by-line */
  while (fscanf (f, "<monitor name=\"%30[^\"]\" lastcheck=\"%d\" />\n",
		 name, (int *) &chk) == 2)
    {
      monmetaptr mm;
      /* fill monmeta struct */
      mm = (monmetaptr) xmlMalloc (sizeof (monmeta));
      mm->seen = 0;
      mm->lastchk = chk;
      xmlHashAddEntry (mef->monitors, name, mm);
      outputf (LVL_DEBUG, "[metafile] Got metadata for %s. Last check was on %s",
	       name, ctime (&chk));
    }
  fclose (f);
  return RET_OK;
}

static void
write_monitor (monmetaptr mm, FILE * f, xmlChar * name)
{
  fprintf (f, "<monitor name=\"%.30s\" lastcheck=\"%d\" />\n",
	   name, (int) mm->lastchk);
}

int
metafile_write (metafileptr mef)
{
  FILE *f;
  f = fopen (mef->filename, "w");
  if (f == NULL)
    {
      outputf (LVL_ERR, "[metafile] Could not open %s for writing\n",
	       mef->filename);
      return RET_ERROR;
    }
  xmlHashScan (mef->monitors, (xmlHashScanner) write_monitor, f);
  fclose (f);
  return RET_OK;
}

void
metafile_close (metafileptr mef)
{
  if (mef == NULL)
    return;
  if (mef->filename != NULL)
    free (mef->filename);
  if (mef->monitors != NULL)
    xmlHashFree (mef->monitors, (xmlHashDeallocator) xmlFree);
  xmlFree (mef);
}

int
monitor_set_last_check (metafileptr mef, const monitorptr m, time_t lastchk)
{
  monmetaptr mm;
  mm = (monmetaptr) xmlHashLookup (mef->monitors, monitor_get_name (m));
  if (mm == NULL)
    {
      mm = (monmetaptr) xmlMalloc (sizeof (monmeta));
      mm->seen = 0;
    }
  mm->lastchk = lastchk;
  return xmlHashUpdateEntry (mef->monitors, monitor_get_name (m), mm, NULL);
}

time_t
monitor_get_last_check (const metafileptr mef, const monitorptr m)
{
  monmetaptr mm;
  mm = (monmetaptr) xmlHashLookup (mef->monitors, monitor_get_name (m));
  return (mm == NULL ? 0 : mm->lastchk);
}

time_t
monitor_get_next_check (const metafileptr mef, const monitorptr m)
{
  return (time_t) (monitor_get_last_check (mef, m) +
		   monitor_get_interval (m));
}
