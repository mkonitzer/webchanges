/* $Id$ */
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>
#include <string.h>
#include "monitor.h"
#include "vpair.h"
#include "global.h"

typedef enum
{
  TR_CHANGED = 0,
  TR_MORE,
  TR_LESS
} trigger;

struct _monitor
{
  /* user-filled variables */
  xmlChar *name;
  xmlChar *xpath;
  unsigned long ival;
  trigger tr_type;
  double tr_prc;
  double tr_add;
  /* state variables */
  vpairptr vp;
  xmlXPathObjectPtr oldres;
  xmlXPathObjectPtr curres;
};

static xmlXPathObjectPtr
evalxpath (xmlDocPtr doc, const xmlChar * expr)
{
  xmlXPathContextPtr ctx;
  xmlXPathObjectPtr obj;
  /* create xpath evaluation context */
  ctx = xmlXPathNewContext (doc);
  if (ctx == NULL)
    {
      outputf (WARN, "[monitor] Could not create xpath context!\n");
      return NULL;
    }
  /* evaluate xpath expression */
  obj = xmlXPathEvalExpression (expr, ctx);
  xmlXPathFreeContext (ctx);
  if (obj == NULL)
    {
      outputf (WARN, "[monitor] Could not evaluate %s!\n", expr);
      return NULL;
    }
  return obj;
}

monitorptr
monitor_new (vpairptr vp, const xmlChar * name)
{
  monitorptr m;
  /* vpair and name must be non-NULL */
  if (vp == NULL || name == NULL)
    return NULL;
  /* fill monitor struct */
  m = xmlMalloc (sizeof (monitor));
  if (m == NULL)
    {
      outputf (ERROR, "[monitor] Out of memory\n");
      return NULL;
    }
  memset (m, 0, sizeof (monitor));
  m->vp = vp;
  if ((m->name = xmlStrdup (name)) == NULL)
    {
      monitor_free (m);
      return NULL;
    }
  return m;
}

int
monitor_evaluate (monitorptr m)
{
  /* monitor must be non-NULL */
  if (m == NULL)
    return RET_ERROR;
  /* parse corresponding vpair */
  if (vpair_parse (m->vp) != 0)
    {
      outputf (NOTICE, "[monitor] Could not parse corresponding vpair\n");
      return RET_ERROR;
    }
  /* old xpath result */
  m->oldres = evalxpath (vpair_get_old_doc (m->vp), m->xpath);
  if (m->oldres == NULL)
    {
      outputf (WARN, "[monitor] Evaluation on old document failed!\n");
      return RET_ERROR;
    }
  /* current xpath result */
  m->curres = evalxpath (vpair_get_cur_doc (m->vp), m->xpath);
  if (m->curres == NULL)
    {
      outputf (WARN, "[monitor] Evaluation on new document failed!\n");
      return RET_ERROR;
    }
  /* are both results of same type */
  if (m->oldres->type != m->curres->type)
    {
      outputf (WARN,
	       "[monitor] Evaluation produced results of different type!\n");
      return RET_ERROR;
    }
  return RET_OK;
}

static int
nodes_equal (xmlNodePtr n1, xmlNodePtr n2)
{
  /* compare memory pointers */
  if (n1 == n2)
    return 1;
  /* nodes must be non-NULL */
  if (n1 == NULL || n2 == NULL)
    return RET_ERROR;
  /* compare types */
  if (n1->type != n2->type)
    return 0;
  switch (n1->type)
    {
    case XML_COMMENT_NODE:
    case XML_TEXT_NODE:
      /* compare contents */
      if (xmlStrEqual (n1->content, n2->content) == 0)
	return 0;
      break;
    case XML_ATTRIBUTE_NODE:
      /* compare attribute contents */
      if (xmlStrEqual (n1->children->content, n2->children->content) == 0)
	return 0;
    case XML_ELEMENT_NODE:
      /* compare names */
      if (xmlStrEqual (n1->name, n2->name) == 0)
	return 0;
    default:
      break;
    }
  /* nodes are equal */
  return 1;
}

static int
results_equal (xmlXPathObjectPtr obj1, xmlXPathObjectPtr obj2)
{
  int i;
  /* results must be non-NULL */
  if (obj1 == NULL || obj2 == NULL)
    return RET_ERROR;
  /* results must be of same type */
  if (obj1->type != obj2->type)
    return RET_ERROR;
  switch (obj1->type)
    {
    case XPATH_NODESET:
      /* results must be non-NULL */
      if (obj1->nodesetval == NULL || obj2->nodesetval == NULL)
	return RET_ERROR;
      if (obj1->nodesetval->nodeNr != obj2->nodesetval->nodeNr)
	return 1;
      for (i = 0; i < obj1->nodesetval->nodeNr; i++)
	if (nodes_equal (obj1->nodesetval->nodeTab[i],
			 obj2->nodesetval->nodeTab[i]) == 0)
	  return 1;
      return 0;
    case XPATH_STRING:
      return (xmlStrcmp (obj1->stringval, obj2->stringval) != 0);
    case XPATH_NUMBER:
      return (obj1->floatval != obj2->floatval);
    case XPATH_BOOLEAN:
      return (obj1->boolval != obj2->boolval);
    default:
      break;
    }
  /* one should never get here */
  return RET_ERROR;
}

int
monitor_triggered (monitorptr m)
{
  double v1, v2;
  /* results must be non-NULL */
  if (m == NULL || m->oldres == NULL || m->curres == NULL)
    return RET_ERROR;
  /* results must be of same type */
  if (m->oldres->type != m->curres->type)
    return RET_ERROR;
  /* special trigger-case: "changed" */
  if (m->tr_type == TR_CHANGED && m->tr_prc == m->tr_add)
    return results_equal (m->oldres, m->curres);
  /* get old (v1) and current (v2) value */
  switch (m->oldres->type)
    {
    case XPATH_NODESET:
      v1 = xmlXPathNodeSetGetLength (m->oldres->nodesetval);
      v2 = xmlXPathNodeSetGetLength (m->curres->nodesetval);
      break;
    case XPATH_STRING:
      v1 = xmlStrlen (m->oldres->stringval);
      v2 = xmlStrlen (m->curres->stringval);
      break;
    case XPATH_NUMBER:
      v1 = m->oldres->floatval;
      v2 = m->curres->floatval;
      break;
    case XPATH_BOOLEAN:
      v1 = (m->oldres->boolval == 0 ? 0 : 1);
      v2 = (m->curres->boolval == 0 ? 0 : 1);
      break;
    default:
      /* one should never get here */
      return RET_ERROR;
    }
  outputf (DEBUG, "[monitor] Comparing %.2lf to %.2lf\n", v1, v2);
  /* compare both values */
  if (m->tr_type == TR_CHANGED)
    /* n[%] "changed" */
    return v2 > v1 * (1 + m->tr_prc / 100) + m->tr_add ||
      v2 < v1 * (1 - m->tr_prc / 100) - m->tr_add;
  else if (m->tr_type == TR_MORE)
    /* [n[%]] "more" */
    return v2 > v1 * (1 + m->tr_prc / 100) + m->tr_add;
  else
    /* [n[%]] "less" */
    return v2 < v1 * (1 - m->tr_prc / 100) - m->tr_add;
}

void
monitor_free (monitorptr m)
{
  if (m == NULL)
    return;
  xmlSafeFree (m->name);
  xmlSafeFree (m->xpath);
  if (m->oldres != NULL)
    xmlXPathFreeObject (m->oldres);
  if (m->curres != NULL)
    xmlXPathFreeObject (m->curres);
  xmlFree (m);
}

int
monitor_set_name (monitorptr m, const xmlChar * name)
{
  xmlSafeFree (m->name);
  return ((m->name = xmlStrdup (name)) == NULL);
}

int
monitor_set_xpath (monitorptr m, const xmlChar * xpath)
{
  xmlSafeFree (m->xpath);
  return ((m->xpath = xmlStrdup (xpath)) == NULL);
}

int
monitor_set_interval (monitorptr m, const xmlChar * ival)
{
  char unit;
  unsigned long val;
  /* parse interval @ival */
  if (sscanf ((char *) ival, "%lu %1c", &val, &unit) != 2)
    {
      outputf (WARN, "[monitor] Invalid interval %s\n", ival);
      return RET_ERROR;
    }
  /* convert @val to seconds */
  switch (unit)
    {
    case 'd':
      val *= 24;
    case 'h':
      val *= 60;
    case 'm':
      val *= 60;
    case 's':
      m->ival = val;
      break;
    default:
      outputf (WARN, "[monitor] Invalid interval unit %c\n", unit);
      return RET_ERROR;
    }
  outputf (DEBUG, "[monitor] Setting interval %s = %lus\n", ival, m->ival);
  return RET_OK;
}

int
monitor_set_trigger (monitorptr m, const xmlChar * trigger)
{
  double val, prc = 0, add = 0;
  xmlChar buf[9], *type = buf;
  /* get trigger value(s) */
  if (sscanf ((char *) trigger, "%lf%% %8s", &val, type) == 2)
    /* n% ("changed"|"more"|"less") */
    prc = val;
  else if (sscanf ((char *) trigger, "%lf %8s", &val, type) == 2)
    /* n ("changed"|"more"|"less") */
    add = val;
  else if (sscanf ((char *) trigger, "%8s", type) != 1)
    {
      /* none of ("changed"|"more"|"less") */
      outputf (WARN, "[monitor] Invalid trigger %s\n", trigger);
      return RET_ERROR;
    }
  /* get trigger type */
  if (xmlStrcasecmp (type, BAD_CAST "changed") == 0)
    m->tr_type = TR_CHANGED;
  else if (xmlStrcasecmp (type, BAD_CAST "more") == 0)
    m->tr_type = TR_MORE;
  else if (xmlStrcasecmp (type, BAD_CAST "less") == 0)
    m->tr_type = TR_LESS;
  else
    {
      outputf (WARN, "[monitor] Invalid trigger type %s\n", trigger);
      return RET_ERROR;
    }
  /* store trigger values */
  m->tr_prc = prc;
  m->tr_add = add;
  outputf (DEBUG, "[monitor] Setting trigger %s (%d,%.2lf,%.2lf)\n",
	   trigger, m->tr_type, m->tr_prc, m->tr_add);
  return RET_OK;
}

xmlChar *
monitor_get_name (monitorptr m)
{
  return m->name;
}

xmlXPathObjectPtr
monitor_get_old_result (monitorptr m)
{
  return m->oldres;
}

xmlXPathObjectPtr
monitor_get_cur_result (monitorptr m)
{
  return m->curres;
}

vpairptr
monitor_get_vpair (monitorptr m)
{
  return m->vp;
}

unsigned int
monitor_get_interval (monitorptr m)
{
  return m->ival;
}
