/* $Id: evalxpath.c 258 2007-03-05 20:12:53Z marius $ */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libxml/HTMLtree.h>
#include <libxml/HTMLparser.h>
#include <libxml/parserInternals.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

char *colnone = "";
char *coltype = "\033[1;35m";
char *colnum = "\033[1;32m";
char *colres = "\033[1;33m";
char *colemph = "\033[1;31m";
char *colnorm = "\033[0m";


static void
ownxmlErrorFunc (void *ctx, const char *msg, ...)
{
#ifdef SHOW_HTML_ERRORS
  va_list args;
  va_start (args, msg);
  vfprintf (stderr, msg, args);
  va_end (args);
#endif /* SHOW_HTML_ERRORS */
}

/*
 * removes leading/multiple/trailing whitespaces from @str
 */
void
normalizespace (xmlChar * str)
{
  int i, o;
  xmlChar blank;
  i = o = blank = 0;
  /* leading */
  while (str[i] != 0 && IS_BLANK_CH (str[i]))
    i++;
  /* multiple */
  while (str[i] != 0)
    {
      if (str[i] == 0xc2 && str[i + 1] == 0xa0)
	{			/* &nbsp; */
	  blank = 0x20;
	  i++;
	}
      else if (IS_BLANK_CH (str[i]))
	{
	  blank = 0x20;
	}
      else
	{
	  if (blank)
	    {
	      str[o++] = blank;
	      blank = 0;
	    }
	  str[o++] = str[i];
	}
      i++;
    }
  /* trailing */
  while (o > 0 && IS_BLANK_CH (str[o - 1]))
    o--;
  str[o] = 0;
}

void
xpathPrintResult (xmlXPathObjectPtr xpathObj)
{
  int i;
  xmlNodePtr cur;
  xmlNodeSetPtr nodes;
  switch (xpathObj->type)
    {
    case XPATH_NODESET:
      /* print nodes in node-set sequentially */
      nodes = xpathObj->nodesetval;
      printf
	("XPath expression resolved to %sNODE-SET%s containing %s%d%s nodes:\n",
	 coltype, colnorm, colnum, (nodes ? nodes->nodeNr : 0), colnorm);
      for (i = 0; i < (nodes ? nodes->nodeNr : 0); i++)
	{
	  cur = nodes->nodeTab[i];
	  printf ("[%s%2d%s] ", colnum, i + 1, colnorm);
	  switch (cur->type)
	    {
	    case XML_ATTRIBUTE_NODE:
	      printf ("(%sATTRIB%s): @%s%s%s=\"%s%s%s\"\n",
		      coltype, colnorm,
		      colres, cur->name, colnorm,
		      colres, cur->children->content, colnorm);
	      break;
	    case XML_COMMENT_NODE:
	      printf ("(%sCOMMENT%s): <!--%s%s%s-->\n",
		      coltype, colnorm, colres, cur->content, colnorm);
	      break;
	    case XML_ELEMENT_NODE:
	      printf ("(%sELEMENT%s): <%s%s%s>\n",
		      coltype, colnorm, colres, cur->name, colnorm);
	      break;
	    case XML_PI_NODE:
	      printf ("(%sPROCINS%s): <?%s%s%s?>\n",
		      coltype, colnorm, colres, cur->content, colnorm);
	      break;
	    case XML_TEXT_NODE:
	      normalizespace (cur->content);
	      printf ("(%sTEXT%s): \"%s%s%s\"\n",
		      coltype, colnorm, colres, cur->content, colnorm);
	      break;
	    default:
	      printf ("(%sTYPE %s%d%s)\n",
		      coltype, colnum, cur->type, colnorm);
	    }
	}
      break;
    case XPATH_BOOLEAN:
      printf ("XPath expression resolved to %sBOOLEAN%s value: %s%s%s\n",
	      coltype, colnorm,
	      colres, xpathObj->boolval ? "TRUE" : "FALSE", colnorm);
      break;
    case XPATH_NUMBER:
      printf ("XPath expression resolved to %sNUMBER%s value: %s%f%s\n",
	      coltype, colnorm, colres, xpathObj->floatval, colnorm);
      break;
    case XPATH_STRING:
      printf ("XPath expression resolved to %sSTRING%s value:\n%s%s%s\n",
	      coltype, colnorm, colres, xpathObj->stringval, colnorm);
      break;
    default:
      printf ("XPath expression resolved to %sUNKNOWN%s variable type\n",
	      coltype, colnorm);
    }
}

int
main (int argc, char **argv)
{
  int nocolor = 1;
  xmlDocPtr doc;
  xmlXPathContextPtr xpathCtx;
  xmlXPathObjectPtr xpathObj;

  /* parse command line */
  if (argc > 1 && strcmp (argv[1], "--color") == 0)
    {
      nocolor = 0;
      argv++;
      argc--;
    }
  if (argc != 3)
    {
      fprintf (stderr, "Usage: evalxpath [--color] Filename XPathExpr\n");
      fprintf (stderr, "   or: evalxpath [--color] URL XPathExpr\n");
      return 1;
    }

  /* init libxml */
  xmlInitParser ();
  LIBXML_TEST_VERSION

  /* register our own error function */
  xmlSetGenericErrorFunc (NULL, ownxmlErrorFunc);

  /* (download and) parse html document */
  /*    doc = htmlReadFile(argv[1], NULL, HTML_PARSE_NOBLANKS);*/
  doc = htmlParseFile (argv[1], NULL);
  if (doc == NULL)
    {
      fprintf (stderr, "ERROR: unable to parse file %s%s%s\n",
	       colemph, argv[1], colnorm);
      return 2;
    }

  /* stamp nodes to speed up xpath evaluation */
  xmlXPathOrderDocElems (doc);

  /* create xpath evaluation context */
  xpathCtx = xmlXPathNewContext (doc);
  if (xpathCtx == NULL)
    {
      fprintf (stderr, "ERROR: unable to create new XPath context\n");
      xmlFreeDoc (doc);
      return -1;
    }

  /* evaluate xpath expression */
  xpathObj = xmlXPathEvalExpression ((unsigned char *) argv[2], xpathCtx);
  if (xpathObj == NULL)
    {
      fprintf (stderr, "ERROR: unable to evaluate xpath expression %s%s%s\n",
	       colemph, argv[2], colnorm);
      xmlXPathFreeContext (xpathCtx);
      xmlFreeDoc (doc);
      return 3;
    }

  /* print result */
  if (nocolor)
    coltype = colnum = colres = colemph = colnorm = colnone;
  xpathPrintResult (xpathObj);


  /* cleanup */
  xmlXPathFreeObject (xpathObj);
  xmlXPathFreeContext (xpathCtx);
  xmlFreeDoc (doc);

  /* shutdown libxml */
  xmlCleanupParser ();

  return 0;
}
