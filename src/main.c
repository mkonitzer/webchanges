/* $Id$ */
/* webchanges -- monitor parts of webpages for changes in content

   Copyright (C) 2006, 2007  Marius Konitzer

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

#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <libxml/list.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "monfile.h"
#include "metafile.h"
#include "monitor.h"
#include "basedir.h"
#include "global.h"
#include "config.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int getopt (int, char *const *, const char *);
extern char *optarg;
extern int optind, opterr, optopt;
#endif /* ! HAVE_GETOPT_H */

int lvl_verbos = NOTICE;	/* level for stdout-verbosity */
int lvl_indent = 0;		/* level for indentation */
int force = 0;			/* force checking */

enum action
{ NONE, CHECK, INIT, UPDATE, REMOVE, TOOMANY };

/*
 * callback output-function
 */
void
outputf (int l, const char *fmt, ...)
{
  int i;
  FILE *f;
  va_list args;
  if (lvl_verbos < l)
    return;
  f = (l != ERROR) ? stdout : stderr;
  for (i = 0; i < lvl_indent; i++)
    fprintf (f, "   ");
  va_start (args, fmt);
  vfprintf (f, fmt, args);
  va_end (args);
}

/*
 * callback in-/outdentation-functions
 */
void
indent (int l)
{
  if (lvl_verbos >= l && lvl_indent < 8)
    lvl_indent++;
}

void
outdent (int l)
{
  if (lvl_verbos >= l && lvl_indent > 0)
    lvl_indent--;
}

/*
 * handle libxml errors
 */
static void
xml_errfunc (void *ctx, const char *msg, ...)
{
#ifdef SHOW_HTML_ERRORS
  va_list args;
  if (verbosity < DEBUG)
    return;
  va_start (args, msg);
  vfprintf (stderr, msg, args);
  va_end (args);
#endif /* SHOW_HTML_ERRORS */
}

static void
xml_strlist_deallocator (xmlLinkPtr lk)
{
  void *data = NULL;
  if (lk == NULL)
    return;
  data = xmlLinkGetData (lk);
  if (data != NULL)
    free (data);
}

/*
 * print results, comparing @oldres to @curres
 */
static void
print_results (int l, xmlXPathObjectPtr oldres, xmlXPathObjectPtr curres)
{
  int i, j;
  /* results comparable? */
  if (oldres->type != curres->type)
    return;
  switch (oldres->type)
    {
    case XPATH_NODESET:
      for (i = 0; i < 2; i++)
	{
	  xmlNodeSetPtr nodes = (i == 0 ? oldres : curres)->nodesetval;
	  outputf (l, "%s node-set:\n", (i == 0 ? "    old" : "current"));
	  /* print nodes in node-set sequentially */
	  for (j = 0; j < (nodes ? nodes->nodeNr : 0); j++)
	    {
	      xmlNodePtr cur = nodes->nodeTab[j];
	      switch (cur->type)
		{
		case XML_ATTRIBUTE_NODE:
		  outputf (l, "[%2d] (ATTR): %s = \"%s\"\n", j + 1,
			   cur->name, cur->children->content);
		  break;
		case XML_COMMENT_NODE:
		  outputf (l, "[%2d] (COMM): %s\n", j + 1, cur->content);
		  break;
		case XML_ELEMENT_NODE:
		  outputf (l, "[%2d] (ELEM): %s\n", j + 1, cur->name);
		  break;
		case XML_TEXT_NODE:
		  outputf (l, "[%2d] (TEXT): %s\n", j + 1, cur->content);
		default:
		  break;
		}
	    }
	}
      break;
    case XPATH_STRING:
      outputf (l, "    old string: %s\n", oldres->stringval);
      outputf (l, "current string: %s\n", curres->stringval);
      break;
    case XPATH_NUMBER:
      outputf (l, "    old number: %.2lf\n", oldres->floatval);
      outputf (l, "current number: %.2lf\n", curres->floatval);
      break;
    case XPATH_BOOLEAN:
      outputf (l, "    old boolean: %.2lf\n",
	       (oldres->boolval == 0 ? "FALSE" : "TRUE"));
      outputf (l, "current boolean: %.2lf\n",
	       (curres->boolval == 0 ? "FALSE" : "TRUE"));
    default:
      break;
    }
}

/*
 * initialize: download all referenced documents
 */
static int
do_init (monfileptr mf)
{
  int ret;
  vpairptr vp;
  /* read monitor file @mf */
  outputf (NOTICE, "Monitor File %s\n", monfile_get_name (mf));
  indent (NOTICE);
  while ((ret = monfile_get_next_vpair (mf, &vp)) != RET_EOF)
    {
      if (ret == RET_ERROR)
	break;
      outputf (NOTICE, "Downloading %s\n", vpair_get_url (vp));
      indent (NOTICE);
      outputf (NOTICE, "=> %s\n", vpair_get_cache (vp));
      vpair_download (vp);
      outdent (NOTICE);
      vpair_close (vp);
    }
  outdent (NOTICE);
  return (ret != RET_ERROR ? RET_OK : RET_ERROR);
}

/*
 * check: print, which monitors have changed and how (and @update cache)
 */
static int
do_check (monfileptr mf, int update)
{
  monitorptr m;
  metafileptr mef;
  const xmlChar *mfname;
  int ret, count = 0;
  vpairptr lastvp = NULL;
  /* read monitor file @mf */
  mfname = monfile_get_name (mf);
  outputf (NOTICE, "Monitor File %s\n", mfname);
  indent (NOTICE);
  /* read metadata file @mef */
  mef = metafile_open (mf);
  metafile_read (mef);
  while ((ret = monfile_get_next_monitor (mf, &m)) != RET_ERROR)
    {
      time_t nextchk;
      const xmlChar *name;
      if (ret == RET_EOF)
	break;
      /* we obtained a monitor @m */
      name = monitor_get_name (m);
      nextchk = monitor_get_next_check (mef, m);
      if (force != 0 || time (NULL) >= nextchk)
	{
	  /* checking of monitor @m is necessary */
	  outputf (NOTICE, "Checking %s now:\n", name);
	  indent (NOTICE);
	  if ((ret = monitor_evaluate (m)) == RET_OK)
	    {
	      /* monitor @m was evaluable */
	      if (monitor_triggered (m) != 0)
		{
		  /* monitor @m reported a change */
		  outputf (WARN, "%s (%s):\n", name, mfname);
		  indent (WARN);
		  print_results (WARN, monitor_get_old_result (m),
				 monitor_get_cur_result (m));
		  outputf (WARN, "\n");
		  outdent (WARN);
		  /* update cache file, if requested */
		  if (update != 0)
		    {
		      vpairptr vp = monitor_get_vpair (m);
		      if (lastvp != vp)
			{
			  /* update of @vp necessary */
			  outputf (NOTICE, "Updating %s\n",
				   vpair_get_cache (vp));
			  indent (NOTICE);
			  vpair_download (vp);
			  outdent (NOTICE);
			  lastvp = vp;
			}
		      else
			outputf (DEBUG, "Skipping update, already done\n");
		    }
		  count++;
		}
	      else
		outputf (NOTICE, "%s NOT triggered.\n", name);
	      monitor_set_last_check (mef, m, time (NULL));
	    }
	  else
	    outputf (WARN, "Skipping %s (%s), not evaluable.\n", name,
		     mfname);
	  outdent (NOTICE);
	}
      else
	outputf (NOTICE, "Skipping %s, next checking %s", name,
		 ctime (&nextchk));
      monitor_free (m);
    }
  /* close metadata file @mef */
  if (ret != RET_ERROR)
    metafile_write (mef);
  metafile_close (mef);
  outdent (NOTICE);
  return (ret != RET_ERROR ? count : RET_ERROR);
}

/*
 * remove: remove referenced files from cache
 */
static int
do_remove (monfileptr mf)
{
  int ret;
  vpairptr vp;
  /* read monitor file @mf */
  outputf (NOTICE, "Monitor File %s\n", monfile_get_name (mf));
  indent (NOTICE);
  while ((ret = monfile_get_next_vpair (mf, &vp)) != RET_EOF)
    {
      if (ret == RET_ERROR)
	break;
      outputf (NOTICE, "Removing %s from cache\n", vpair_get_url (vp));
      indent (NOTICE);
      vpair_remove (vp);
      outdent (NOTICE);
      vpair_close (vp);
    }
  outdent (NOTICE);
  return (ret != RET_ERROR ? RET_OK : RET_ERROR);
}

static void
usage (FILE * f)
{
  fprintf (f, "Usage: webchanges COMMAND [OPTION]... FILE...\n\n");
  fprintf (f, "Commands:\n");
  fprintf (f, "  -i  initialize monitor file, download into cache\n");
  fprintf (f, "  -c  check monitor file for changes\n");
  fprintf (f, "  -u  check monitor file for changes and update cache\n");
  fprintf (f, "  -r  remove files associated with monitor file from cache\n");
  fprintf (f, "  -h  display this help and exit\n");
  fprintf (f, "  -V  display version & copyright information and exit\n\n");
  fprintf (f, "Options:\n");
  fprintf (f, "  -f  force checking/updating of all monitors now\n");
  fprintf (f, "  -b  set base directory\n");
  fprintf (f, "  -q  quiet mode, suppress most stdout messages\n");
  fprintf (f, "  -v  verbose mode, repeat to increase stdout messages\n");
}

static void
version (void)
{
  printf ("webchanges version %s\n", VERSION);
  printf ("Copyright (C) 2006, 2007 Marius Konitzer\n");
  printf ("This is free software; see the source for copying conditions.  ");
  printf ("There is NO\nwarranty; not even for MERCHANTABILITY or FITNESS ");
  printf ("FOR A PARTICULAR PURPOSE,\nto the extent permitted by law.\n");
}

static int
errexit (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  vfprintf (stderr, fmt, args);
  va_end (args);
  fprintf (stderr, "\n");
  usage (stderr);
  return -1;
}

int
main (int argc, char **argv)
{
  int c, count = 0;
  int action = NONE;
  basedirptr basedir = NULL;
  char *userdir = NULL;
  xmlListPtr filelist = NULL;

  /* register error function */
  xmlSetGenericErrorFunc (NULL, xml_errfunc);

  /* parse cmdline args */
  opterr = 0;			/* prevent getopt from printing errors */
  while ((c = getopt (argc, argv, "icurhVfb:qv")) != -1)
    {
      switch (c)
	{
	  /* commands */
	case 'i':		/* init */
	  action = (action == NONE ? INIT : TOOMANY);
	  break;
	case 'c':		/* check */
	  action = (action == NONE ? CHECK : TOOMANY);
	  break;
	case 'u':		/* update */
	  action = (action == NONE ? UPDATE : TOOMANY);
	  break;
	case 'r':		/* remove */
	  action = (action == NONE ? REMOVE : TOOMANY);
	  break;
	case 'h':		/* help */
	  usage (stdout);
	  return 0;
	case 'V':		/* version info */
	  version ();
	  return 0;
	  /* options */
	case 'f':		/* force */
	  force = 1;
	  break;
	case 'b':		/* base directory */
	  if (userdir != NULL)
	    return
	      errexit ("More than one base directory specified, exiting.");
	  userdir = strdup (optarg);
	  break;
	case 'v':		/* verbose */
	  lvl_verbos++;
	  break;
	case 'q':		/* quiet */
	  lvl_verbos = WARN;
	  break;
	  /* errors */
	case '?':
	  if (userdir != NULL)
	    {
	      free (userdir);
	      userdir = NULL;
	    }
	  return errexit ("Unknown option '-%c'.", optopt);
	}
    }

  /* do we have a unique command? */
  if (action == NONE)
    return errexit ("No command given, exiting.");
  if (action == TOOMANY)
    return errexit ("More than one command given, exiting.");

  /* Setup basedir */
  basedir = basedir_open (userdir);
  if (userdir != NULL)
    {
      free (userdir);
      userdir = NULL;
    }
  if (basedir == NULL)
    return errexit ("Could not determine base directory.");
  /* Monitor files must be passed on cmdline when using current directory. */
  if (basedir_is_curdir (basedir) && argc - optind == 0)
    {
      basedir_close (basedir);
      return errexit ("No monitor file(s) given, exiting.");
    }
  /* Prepare base directory if necessary. */
  if (basedir_is_prepared (basedir) == 0)
    {
      if (basedir_prepare (basedir) != RET_OK)
	{
	  basedir_close (basedir);
	  return errexit ("Could not prepare base directory, exiting.");
	}
    }

  /* Build file list ... */
  filelist = xmlListCreate (xml_strlist_deallocator, NULL);
  if (argc - optind > 0)
    {
      int i;
      /* ... either from the user-specified files on cmdline ... */
      for (i = optind; i < argc; i++)
	xmlListPushBack (filelist, strdup (argv[i]));
    }
  else
    /* ... or by taking all monitor files in basedir. */
    basedir_get_all_monfiles (basedir, filelist);
  if (xmlListEmpty (filelist) != 0)
    {
      basedir_close (basedir);
      return errexit ("No monitor files found, exiting.");
    }

  /* Walk through all monitor files found. */
  while (xmlListEmpty (filelist) == 0)
    {
      int ret = 0;
      monfileptr mf;
      xmlLinkPtr lk;
      const char *filename;
      /* Get next monitor file from file list */
      lk = xmlListFront (filelist);
      filename = (const char*) xmlLinkGetData (lk);

      /* Open monitor file. */
      mf = monfile_open (filename, basedir);
      if (mf == NULL)
	{
	  /* Skip this monitor file. */
	  outputf (ERROR, "Error reading monitor file %s\n\n", filename);
	  xmlListPopFront (filelist);
	  count = -1;
	  continue;
	}
      /* Process monitor file. */
      outputf (INFO, "Processing monitor file %s\n", filename);
      switch (action)
	{
	case INIT:
	  ret = do_init (mf);
	  break;
	case CHECK:
	  ret = do_check (mf, 0);
	  break;
	case UPDATE:
	  ret = do_check (mf, 1);
	  break;
	case REMOVE:
	  ret = do_remove (mf);
	  break;
	}
      count = (ret < 0 || count < 0 ? -1 : count + ret);
      /* Ready to close monitor file. */
      monfile_close (mf);
      xmlListPopFront (filelist);
    }
  basedir_close (basedir);
  xmlListDelete (filelist);
  xmlCleanupParser ();
  return count;
}
