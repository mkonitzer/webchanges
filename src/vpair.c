/* $Id$ */
/* Everything dealing with a version pair

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <libxml/HTMLparser.h>
#include <libxml/xmlIO.h>
#include <libxml/tree.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif
#include "global.h"
#include "vpair.h"
#include "sha1.h"
#include "basedir.h"


struct _vpair
{
  /* user-filled variables */
  xmlChar *url;
  /* state variables */
  char *cache;
  xmlParserInputBufferPtr curbuf;
  xmlDocPtr curdoc;
  xmlDocPtr olddoc;
};

static char *
url_to_cache (const xmlChar * url)
{
  int i;
  unsigned char hashval[20];
  char *hash, *pos;
  hash = pos = (char *) malloc (2 * 20 + 5 + 1);
  sha1_buffer ((char *) url, strlen ((char *) url), hashval);
  for (i = 0; i < 20; i++)
    {
      sprintf (pos, "%02x", hashval[i]);
      pos += 2;
    }
  return strcat (hash, ".html");
}

vpairptr
vpair_open (const xmlChar * url, const basedirptr bd)
{
  char *filename = NULL;
  vpairptr vp;
  /* allocate vpair struct */
  vp = (vpairptr) xmlMalloc (sizeof (vpair));
  if (vp == NULL)
    {
      outputf (LVL_ERR, "[vpair] Out of memory\n");
      return NULL;
    }
  /* fill vpair struct */
  memset (vp, 0, sizeof (vpair));
  vp->url = xmlStrdup (url);
  outputf (LVL_DEBUG, "[vpair] Using current document %s\n", vp->url);
  /* calculate cache filename */
  filename = url_to_cache (vp->url);
  vp->cache = basedir_buildpath_cache (bd, filename);
  outputf (LVL_DEBUG, "[vpair] Using old document %s\n", vp->cache);
  free (filename);
  return vp;
}

#ifdef HAVE_LIBCURL
static size_t
curl2libxml_writer (void *ptr, size_t size, size_t nmemb, void *stream)
{
  xmlParserInputBufferPtr buf = (xmlParserInputBufferPtr) stream;
  if (buf == NULL)
    return -1;
  return xmlParserInputBufferPush (buf, size * nmemb, (const char *) ptr);
}
#endif

static xmlParserInputBufferPtr
read_into_buffer (const char *filename)
{
  xmlParserInputBufferPtr buf;
#ifdef HAVE_LIBCURL
  CURL *curl;
  CURLcode res;

  /* open document */
  buf = xmlAllocParserInputBuffer (XML_CHAR_ENCODING_NONE);

  /* read document */
  if ((curl = curl_easy_init ()) == NULL)
    {
      outputf (LVL_ERR, "[vpair] Unable to initialize curl\n");
      return NULL;
    }
  curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 1);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, &curl2libxml_writer);
  curl_easy_setopt (curl, CURLOPT_WRITEDATA, buf);
  curl_easy_setopt (curl, CURLOPT_URL, filename);
  res = curl_easy_perform (curl);
  curl_easy_cleanup (curl);
#else
  int read;

  /* open document */
  if ((buf =
       xmlParserInputBufferCreateFilename (filename,
					   XML_CHAR_ENCODING_NONE)) == NULL)
    {
      outputf (LVL_WARN, "[vpair] Could not open %s\n", filename);
      return NULL;
    }
  /* read document */
  while ((read = xmlParserInputBufferRead (buf, 2048)) > 0);
  if (read < 0)
    {
      outputf (LVL_WARN, "[vpair] Error reading %s\n", filename);
      xmlFreeParserInputBuffer (buf);
      return NULL;
    }
#endif
  return buf;
}

int
vpair_parse (vpairptr vp)
{
  /* read and parse old document (do not keep in memory) */
  outputf (LVL_INFO, "[vpair] Fetching cached document %s\n", vp->cache);
  if ((vp->olddoc = htmlReadFile (vp->cache, NULL, 0)) == NULL)
    {
      outputf (LVL_WARN, "[vpair] Could not parse %s\n", vp->cache);
      return RET_ERROR;
    }
  /* read current document (and keep in memory) */
  outputf (LVL_INFO, "[vpair] Fetching document %s\n", vp->url);
  if ((vp->curbuf = read_into_buffer ((char *) vp->url)) == NULL)
    {
      outputf (LVL_WARN, "[vpair] Could not open %s\n", vp->url);
      xmlFreeDoc (vp->olddoc);
      vp->olddoc = NULL;
      return RET_ERROR;
    }
  /* parse current document */
  if ((vp->curdoc =
       htmlReadMemory ((char *) xmlBufferContent (vp->curbuf->buffer),
		       xmlBufferLength (vp->curbuf->buffer),
		       (const char *) vp->url, NULL, 0)) == NULL)
    {
      outputf (LVL_WARN, "[vpair] Could not parse %s\n", vp->url);
      xmlFreeParserInputBuffer (vp->curbuf);
      vp->curbuf = NULL;
      xmlFreeDoc (vp->olddoc);
      vp->olddoc = NULL;
      return RET_ERROR;
    }
  return RET_OK;
}

int
vpair_download (vpairptr vp)
{
  int written;
  xmlOutputBufferPtr output;
  /* read current document (if necessary) */
  if (vp->curbuf == NULL)
    {
      outputf (LVL_INFO, "[vpair] Fetching document %s\n", vp->url);
      if ((vp->curbuf = read_into_buffer ((char *) vp->url)) == NULL)
	{
	  outputf (LVL_WARN, "[vpair] Could not open %s\n", vp->url);
	  return RET_ERROR;
	}
    }
  /* open cache */
  if ((output = xmlOutputBufferCreateFilename (vp->cache, NULL, 0)) == NULL)
    {
      outputf (LVL_WARN, "[vpair] Could not open %s\n", vp->cache);
      return RET_ERROR;
    }
  /* write current document to cache */
  written =
    xmlOutputBufferWrite (output, xmlBufferLength (vp->curbuf->buffer),
			  (char *) xmlBufferContent (vp->curbuf->buffer));
  /* close cache */
  xmlOutputBufferClose (output);
  if (written == -1)
    {
      outputf (LVL_WARN, "[vpair] Error writing to %s\n", vp->cache);
      return RET_ERROR;
    }
  outputf (LVL_INFO, "[vpair] Successfully downloaded %s to %s\n",
	   vp->url, vp->cache);
  return RET_OK;
}

int
vpair_remove (vpairptr vp)
{
  if (remove (vp->cache) != 0)
    {
      outputf (LVL_WARN, "[vpair] Could not remove %s\n", vp->cache);
      return RET_ERROR;
    }
  outputf (LVL_INFO, "[vpair] Successfully removed %s\n", vp->cache);
  return RET_OK;
}

void
vpair_close (vpairptr vp)
{
  if (vp == NULL)
    return;
  if (vp->curbuf != NULL)
    xmlFreeParserInputBuffer (vp->curbuf);
  if (vp->olddoc != NULL)
    xmlFreeDoc (vp->olddoc);
  if (vp->curdoc != NULL)
    xmlFreeDoc (vp->curdoc);
  xmlSafeFree (vp->url);
  xmlSafeFree (vp->cache);
  xmlSafeFree (vp);
}

const xmlChar *
vpair_get_url (const vpairptr vp)
{
  return vp->url;
}

const char *
vpair_get_cache (const vpairptr vp)
{
  return vp->cache;
}

xmlDocPtr
vpair_get_old_doc (const vpairptr vp)
{
  return vp->olddoc;
}

xmlDocPtr
vpair_get_cur_doc (const vpairptr vp)
{
  return vp->curdoc;
}
