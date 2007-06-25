/* $Id$ */
#include <libxml/xmlstring.h>
#include <libxml/hash.h>
#include <string.h>
#include <time.h>
#include "metafile.h"
#include "monitor.h"
#include "global.h"

struct _metafile
{
  /* user-filled variables */
  monfileptr mf;
  /* state variables */
  xmlChar *filename;
  xmlHashTablePtr monitors;
};

typedef struct
{
  int seen;
  time_t lastchk;
} monmeta;
typedef monmeta *monmetaptr;

static xmlChar *
monfile_to_metafile (xmlChar * filename)
{
  int len, i;
  xmlChar *ret;
  len = xmlStrlen (filename);
  for (i = 1; i < 6; i++)
    if (filename[len - i] == '.')
      break;
  ret = xmlStrndup (filename, len - i % 6);
  ret = xmlStrncat (ret, BAD_CAST ".meta", 5);
  return ret;
}

metafileptr
metafile_open (const monfileptr mf)
{
  metafileptr mef;
  /* allocate metafile struct */
  mef = xmlMalloc (sizeof (metafile));
  if (mef == NULL)
    {
      outputf (ERROR, "[metafile] Out of memory\n");
      return NULL;
    }
  /* fill metafile struct */
  memset (mef, 0, sizeof (metafile));
  mef->mf = mf;
  /* calculate meta filename */
  mef->filename = monfile_to_metafile (monfile_get_filename (mf));
  outputf (DEBUG, "[metafile] Using metadata file %s\n", mef->filename);
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
  f = fopen ((char *) mef->filename, "r");
  if (f == NULL)
    {
      outputf (ERROR, "[metafile] Could not open %s for reading\n",
	       mef->filename);
      return RET_ERROR;
    }
  /* read metadata file line-by-line */
  while (fscanf (f, "<monitor name=\"%30[^\"]\" lastcheck=\"%d\" />\n",
		 name, (int *) &chk) == 2)
    {
      monmeta *mm;
      /* fill monmeta struct */
      mm = xmlMalloc (sizeof (monmeta));
      mm->seen = 0;
      mm->lastchk = chk;
      xmlHashAddEntry (mef->monitors, name, mm);
      outputf (DEBUG, "[metafile] Got metadata for %s. Last check was on %s",
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
  f = fopen ((char *) mef->filename, "w");
  if (f == NULL)
    {
      outputf (ERROR, "[metafile] Could not open %s for writing\n",
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
  xmlSafeFree (mef->filename);
  if (mef->monitors != NULL)
    xmlHashFree (mef->monitors, (xmlHashDeallocator) xmlFree);
  xmlFree (mef);
}

int
monitor_set_last_check (metafileptr mef, monitorptr m, time_t lastchk)
{
  monmetaptr mm;
  mm = xmlHashLookup (mef->monitors, monitor_get_name (m));
  if (mm == NULL)
    {
      mm = xmlMalloc (sizeof (monmeta));
      mm->seen = 0;
    }
  mm->lastchk = lastchk;
  return xmlHashUpdateEntry (mef->monitors, monitor_get_name (m), mm, NULL);
}

time_t
monitor_get_last_check (metafileptr mef, monitorptr m)
{
  monmetaptr mm;
  mm = xmlHashLookup (mef->monitors, monitor_get_name (m));
  return (mm == NULL ? 0 : mm->lastchk);
}

time_t
monitor_get_next_check (metafileptr mef, monitorptr m)
{
  return (time_t) (monitor_get_last_check (mef, m) +
		   monitor_get_interval (m));
}
