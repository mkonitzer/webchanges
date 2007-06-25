/* $Id$ */
#include <libxml/HTMLparser.h>
#include <libxml/xmlIO.h>
#include <string.h>
#include <errno.h>
#include "global.h"
#include "vpair.h"
#include "sha.h"


struct _vpair
{
  /* user-filled variables */
  xmlChar *url;
  /* state variables */
  xmlChar *cache;
  xmlParserInputBufferPtr curbuf;
  xmlDocPtr curdoc;
  xmlDocPtr olddoc;
};

static xmlChar *
url_to_cache (const xmlChar * url)
{
  int i;
  unsigned char hashval[20];
  xmlChar *hash, *pos, *ret;
  hash = pos = xmlMalloc (41);
  shaBlock ((unsigned char *) url, strlen ((char *) url), hashval);
  for (i = 0; i < 20; i++)
    {
      sprintf ((char *) pos, "%02x", hashval[i]);
      pos += 2;
    }
  ret = xmlStrncatNew (hash, BAD_CAST ".html", 5);
  xmlSafeFree (hash);
  return ret;
}

vpairptr
vpair_open (const xmlChar * url)
{
  vpairptr vp;
  /* allocate vpair struct */
  vp = xmlMalloc (sizeof (vpair));
  if (vp == NULL)
    {
      outputf (ERROR, "[vpair] Out of memory\n");
      return NULL;
    }
  /* fill vpair struct */
  memset (vp, 0, sizeof (vpair));
  vp->url = xmlStrdup (url);
  outputf (DEBUG, "[vpair] Using current document %s\n", vp->url);
  /* calculate cache filename */
  vp->cache = url_to_cache (vp->url);
  outputf (DEBUG, "[vpair] Using old document %s\n", vp->cache);
  return vp;
}

static xmlParserInputBufferPtr
read_into_buffer (const char *filename)
{
  int read;
  xmlParserInputBufferPtr buf;
  /* open document */
  if ((buf = xmlParserInputBufferCreateFilename ((char *) filename,
						 XML_CHAR_ENCODING_NONE))
      == NULL)
    {
      outputf (WARN, "[vpair] Could not open %s\n", filename);
      return NULL;
    }
  /* read document */
  while ((read = xmlParserInputBufferRead (buf, 2048)) > 0);
  if (read < 0)
    {
      outputf (WARN, "[vpair] Error reading %s\n", filename);
      xmlFreeParserInputBuffer (buf);
      return NULL;
    }
  return buf;
}

int
vpair_parse (vpairptr vp)
{
  /* read and parse old document (do not keep in memory) */
  outputf (INFO, "[vpair] Fetching cached document %s\n", vp->cache);
  if ((vp->olddoc = htmlReadFile ((char *) vp->cache, NULL, 0)) == NULL)
    {
      outputf (WARN, "[vpair] Could not parse %s\n", vp->cache);
      return RET_ERROR;
    }
  /* read current document (and keep in memory) */
  outputf (INFO, "[vpair] Fetching document %s\n", vp->url);
  if ((vp->curbuf = read_into_buffer ((char *) vp->url)) == NULL)
    {
      outputf (WARN, "[vpair] Could not open %s\n", vp->url);
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
      outputf (WARN, "[vpair] Could not parse %s\n", vp->url);
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
      outputf (INFO, "[vpair] Fetching document %s\n", vp->url);
      if ((vp->curbuf = read_into_buffer ((char *) vp->url)) == NULL)
	{
	  outputf (WARN, "[vpair] Could not open %s\n", vp->url);
	  return RET_ERROR;
	}
    }
  /* open cache */
  if ((output = xmlOutputBufferCreateFilename ((char *) vp->cache,
					       NULL, 0)) == NULL)
    {
      outputf (WARN, "[vpair] Could not open %s\n", vp->cache);
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
      outputf (WARN, "[vpair] Error writing to %s\n", vp->cache);
      return RET_ERROR;
    }
  outputf (INFO, "[vpair] Successfully downloaded %s to %s\n",
	   vp->url, vp->cache);
  return RET_OK;
}

int
vpair_remove (vpairptr vp)
{
  if (remove ((char *) vp->cache) != 0)
    {
      outputf (WARN, "[vpair] Could not remove %s\n", vp->cache);
      return RET_ERROR;
    }
  outputf (INFO, "[vpair] Successfully removed %s\n", vp->cache);
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
  return;
}

xmlChar *
vpair_get_url (vpairptr vp)
{
  return vp->url;
}

xmlChar *
vpair_get_cache (vpairptr vp)
{
  return vp->cache;
}

xmlDocPtr
vpair_get_old_doc (vpairptr vp)
{
  return vp->olddoc;
}

xmlDocPtr
vpair_get_cur_doc (vpairptr vp)
{
  return vp->curdoc;
}
