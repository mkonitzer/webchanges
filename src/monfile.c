/* $Id$ */
/* Parse a given monitor file

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

#include <libxml/xmlreader.h>
#include <libxml/xmlstring.h>
#include <libxml/parser.h>
#include <string.h>
#include "monfile_dtd.inc"
#include "monfile.h"
#include "monitor.h"
#include "vpair.h"
#include "basedir.h"
#include "global.h"

struct _monfile
{
  /* user-filled variables */
  char *filename;
  char *fullpath;
  xmlChar *name;
  basedirptr bd;
  /* state variables */
  xmlTextReaderPtr reader;
  vpairptr vp;
};

static xmlExternalEntityLoader default_loader = NULL;

static void
struct_error (void *userData, xmlErrorPtr error)
{
  outputf (LVL_ERR, "[monfile] Error at line %d, level %d: %s\n", error->line,
	   error->level, error->message);
}

static xmlParserInputPtr
entity_loader (const char *url, const char *id, xmlParserCtxtPtr ctx)
{
  xmlParserInputBufferPtr inp;
  if (url == NULL || strcmp (url, MONFILE_DTD_URL) != 0)
    {
      outputf (LVL_ERR, "[monfile] Wrong DTD %s, please use %s instead!\n",
	       url, MONFILE_DTD_URL);
      return NULL;
    }
  /* restore default entity loader */
  xmlSetExternalEntityLoader (default_loader);
  /* pass webchanges DTD to libxml */
  inp = xmlParserInputBufferCreateMem (monfile_dtd,
				       strlen (monfile_dtd),
				       XML_CHAR_ENCODING_UTF8);
  return xmlNewIOInputStream (ctx, inp, XML_CHAR_ENCODING_UTF8);
}

monfileptr
monfile_open (const char *filename, const basedirptr bd)
{
  monfileptr mf;
  if (filename == NULL || bd == NULL)
    return NULL;
  /* allocate monfile struct */
  mf = (monfileptr) xmlMalloc (sizeof (monfile));
  if (mf == NULL)
    {
      outputf (LVL_ERR, "[monfile] Out of memory\n");
      return NULL;
    }
  /* fill monfile struct */
  memset (mf, 0, sizeof (monfile));
  mf->filename = strdup (filename);
  mf->fullpath = basedir_buildpath_monfile (bd, filename);
  mf->bd = bd;
  /* register entity loader */
  if (default_loader == NULL)
    default_loader = xmlGetExternalEntityLoader ();
  xmlSetExternalEntityLoader (entity_loader);
  /* open monitor file for validated parsing */
  mf->reader = xmlReaderForFile (mf->fullpath, NULL, XML_PARSE_NOENT |
				 XML_PARSE_DTDATTR | XML_PARSE_DTDVALID);
  if (mf->reader == NULL)
    {
      outputf (LVL_ERR, "[monfile] Could not open %s\n", filename);
      monfile_close (mf);
      return NULL;
    }
  /* register our error function */
  xmlTextReaderSetStructuredErrorHandler (mf->reader, struct_error, NULL);
  /* process monfile @mf node-by-node (to get monfile name) */
  while (xmlTextReaderRead (mf->reader) == 1)
    {
      const xmlChar *name;
      /* try to read from monfile */
      /* do we have a valid monfile up to now? */
      if (xmlTextReaderIsValid (mf->reader) != 1)
	{
	  outputf (LVL_ERR, "[monfile] Failed to validate!\n");
	  monfile_close (mf);
	  return NULL;
	}
      /* do we have a <monitorfile name="...">? */
      if (xmlTextReaderNodeType (mf->reader) == XML_READER_TYPE_ELEMENT &&
	  (name = xmlTextReaderConstName (mf->reader)) != NULL &&
	  xmlStrEqual (name, BAD_CAST "monitorfile") == 1)
	{
	  mf->name = xmlTextReaderGetAttribute (mf->reader, BAD_CAST "name");
	  break;
	}
    }
  if (mf->name == NULL)
    {
      outputf (LVL_ERR, "[monfile] Failed to read!\n");
      monfile_close (mf);
      return NULL;
    }
  return mf;
}

int
monfile_get_next_vpair (const monfileptr mf, vpairptr * vp)
{
  int read;
  /* process monfile @mf node-by-node (to get vpair) */
  while ((read = xmlTextReaderRead (mf->reader)) == 1)
    {
      const xmlChar *name;
      /* do we have a valid monfile @mf up to now? */
      if (xmlTextReaderIsValid (mf->reader) != 1)
	{
	  outputf (LVL_ERR, "[monfile] Failed to validate!\n");
	  return RET_ERROR;
	}
      name = xmlTextReaderConstName (mf->reader);
      switch (xmlTextReaderNodeType (mf->reader))
	{
	case XML_READER_TYPE_ELEMENT:
	  outputf (LVL_DEBUG, "[monfile] Got <%s...\n", name);
	  if (xmlStrEqual (name, BAD_CAST "document") == 1)
	    {
	      xmlChar *url = xmlTextReaderGetAttribute (mf->reader,
							BAD_CAST "url");
	      /* open version pair */
	      mf->vp = vpair_open (url, mf->bd);
	      xmlSafeFree (url);
	      if (mf->vp == NULL)
		return RET_WARNING;
	    }
	  break;
	case XML_READER_TYPE_END_ELEMENT:
	  outputf (LVL_DEBUG, "[monfile] Got </%s>\n", name);
	  if (xmlStrEqual (name, BAD_CAST "document") == 1)
	    {
	      /* complete version pair - ready! */
	      *vp = mf->vp;
	      return RET_OK;
	    }
	  break;
	}

    }
  /* read error */
  if (read == -1)
    {
      outputf (LVL_ERR, "[monfile] Failed to read!\n");
      return RET_ERROR;
    }
  /* end-of-file */
  return RET_EOF;
}

int
monfile_get_next_monitor (const monfileptr mf, monitorptr * mon)
{
  int read, skipdoc = 0;
  monitorptr m = NULL;
  xmlChar *lasttext = NULL;
  /* process monfile @mf node-by-node (to get monitor) */
  while ((read = xmlTextReaderRead (mf->reader)) == 1)
    {
      const xmlChar *name;
      /* do we have a valid monfile @mf up to now? */
      if (xmlTextReaderIsValid (mf->reader) != 1)
	{
	  outputf (LVL_ERR, "[monfile] Failed to validate!\n");
	  xmlSafeFree (lasttext);
	  return RET_ERROR;
	}
      name = xmlTextReaderConstName (mf->reader);
      switch (xmlTextReaderNodeType (mf->reader))
	{
	case XML_READER_TYPE_ELEMENT:
	  outputf (LVL_DEBUG, "[monfile] Got <%s ...>\n", name);
	  if (xmlStrEqual (name, BAD_CAST "document") == 1)
	    {
	      xmlChar *url = xmlTextReaderGetAttribute (mf->reader,
							BAD_CAST "url");
	      /* open version pair */
	      mf->vp = vpair_open (url, mf->bd);
	      xmlSafeFree (url);
	      if (mf->vp == NULL)
		{
		  /* skip this <document>-block */
		  outputf (LVL_NOTICE, "[monfile] Skipping document\n");
		  skipdoc = 1;
		  xmlSafeFree (lasttext);
		  return RET_WARNING;
		}
	    }
	  else if (xmlStrEqual (name, BAD_CAST "monitor") == 1)
	    {
	      xmlChar *name;
	      if (skipdoc)
		break;
	      name = xmlTextReaderGetAttribute (mf->reader, BAD_CAST "name");
	      /* create new monitor @m */
	      m = monitor_new (mf->vp, name);
	      xmlSafeFree (name);
	      if (m == NULL)
		{
		  xmlSafeFree (lasttext);
		  return RET_WARNING;
		}
	    }
	  break;
	case XML_READER_TYPE_END_ELEMENT:
	  outputf (LVL_DEBUG, "[monfile] Got </%s>\n", name);
	  if (xmlStrEqual (name, BAD_CAST "document") == 1)
	    {
	      skipdoc = 0;
	      vpair_close (mf->vp);
	      mf->vp = NULL;
	    }
	  else if (xmlStrEqual (name, BAD_CAST "monitor") == 1)
	    {
	      if (skipdoc)
		break;
	      /* complete monitor @m - ready! */
	      xmlSafeFree (lasttext);
	      *mon = m;
	      return RET_OK;
	    }
	  else if (xmlStrEqual (name, BAD_CAST "xpath") == 1)
	    {
	      if (skipdoc)
		break;
	      monitor_set_xpath (m, lasttext);
	    }
	  else if (xmlStrEqual (name, BAD_CAST "interval") == 1)
	    {
	      if (skipdoc)
		break;
	      monitor_set_interval (m, lasttext);
	    }
	  else if (xmlStrEqual (name, BAD_CAST "trigger") == 1)
	    {
	      if (skipdoc)
		break;
	      monitor_set_trigger (m, lasttext);
	    }
	  break;
	case XML_READER_TYPE_TEXT:
	  if (skipdoc)
	    break;
	  xmlSafeFree (lasttext);
	  lasttext = xmlStrdup (xmlTextReaderConstValue (mf->reader));
	  break;
	}
    }
  /* read error */
  if (read == -1)
    {
      outputf (LVL_ERR, "[monfile] Failed to read!\n");
      return RET_ERROR;
    }
  /* end-of-file */
  return RET_EOF;
}

void
monfile_close (monfileptr mf)
{
  if (mf == NULL)
    return;
  if (mf->filename != NULL)
    free (mf->filename);
  if (mf->fullpath != NULL)
    free (mf->fullpath);
  xmlSafeFree (mf->name);
  if (mf->reader != NULL)
    xmlFreeTextReader (mf->reader);
  xmlFree (mf);
}

const char *
monfile_get_filename (const monfileptr mf)
{
  return mf->filename;
}

const xmlChar *
monfile_get_name (const monfileptr mf)
{
  return mf->name;
}

basedirptr
monfile_get_basedir (const monfileptr mf)
{
  return (mf == NULL ? NULL : mf->bd);
}
