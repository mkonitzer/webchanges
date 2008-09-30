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

#include <libxml/xmlmemory.h>
#include <libxml/list.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#ifndef _WIN32
#include <pwd.h>
#endif
#include "basedir.h"
#include "global.h"

struct _basedir
{
  char *base_dir;		/* base config directory (top) */
  /* directory containing the ... */
  char *monfile_dir;		/* ... monitor files */
  char *metafile_dir;		/* ... metadata files */
  char *cache_dir;		/* ... cached versions */
};

/*
 * Build pathname from cur and add (e.g. 'cur/add').
 */
static char *
dir_join (const char *cur, const char *add)
{
  if (cur == NULL || add == NULL)
    return NULL;
  char *joined = (char*) malloc (strlen (cur) + 1 + strlen (add) + 1);
  strcpy (joined, cur);
#ifdef _WIN32
  strcat (joined, "\\");
#else
  strcat (joined, "/");
#endif
  return strcat (joined, add);
}

/*
 * Checks if the specified directory exists.
 */
static int
dir_exists (const char *dirname)
{
  struct stat st;
  if (stat (dirname, &st) != 0)
    return 0;
  if (S_ISDIR (st.st_mode) == 0)
    return 0;
  return 1;
}

/*
 * Checks if the specified file exists.
 */
static int
file_exists (const char *filename)
{
  struct stat st;
  if (stat (filename, &st) != 0)
    return 0;
  if (S_ISREG (st.st_mode) == 0)
    return 0;
  return 1;
}

/*
 * Get the default base directory where monitor/meta/cache files reside.
 */
static char *
get_default_basedir (void)
{
  char *basedir;
#ifdef _WIN32
  char *appdatadir = NULL;
  const char *profiledir = NULL;
#else
  const char *homedir = NULL;
  struct passwd *pwd = NULL;
#endif
#ifdef _WIN32
  /* Find location of user profile (rather than home directory). */
  appdatadir = getenv ("APPDATA");
  if (appdatadir == NULL)
    {
      profiledir = getenv ("USERPROFILE");
      if (profiledir == NULL)
	return NULL;
      appdatadir = dir_join (profiledir, "Application Data");
      basedir = dir_join (appdatadir, "webchanges");
      free (appdatadir);
      appdatadir = NULL;
    }
  else
    basedir = dir_join (appdatadir, "webchanges");
#else
  /* If $HOME is set, use that. */
  homedir = getenv ("HOME");
  if (homedir == NULL)
    {
      pwd = getpwuid (getuid ());
      if (pwd == NULL || pwd->pw_dir == NULL)
	return NULL;
      homedir = pwd->pw_dir;
    }
  basedir = dir_join (homedir, ".webchanges");
#endif

  return basedir;
}

basedirptr
basedir_open (const char *dirname)
{
  basedirptr bd;
  /* Allocate basedir struct. */
  bd = (basedirptr) xmlMalloc (sizeof (basedir));
  if (bd == NULL)
    {
      outputf (ERROR, "[basedir] Out of memory\n");
      return NULL;
    }
  memset (bd, 0, sizeof (basedir));

  if (dirname != NULL)
    {
      /* Try to take user-specified directory */
      if (strcmp (dirname, ".") == 0)
	{
	  /* Special case: "." means "take current directory */
	  outputf (INFO,
		   "[basedir] Using current directory as base directory.\n");
	  return bd;
	}
      if (dir_exists (dirname) == 0)
	{
	  outputf (ERROR,
		   "[basedir] Base directory '%s' does not exist, please create it first.\n",
		   dirname);
	  xmlFree (bd);
	  return NULL;
	}
      outputf (INFO, "[basedir] Using base directory '%s'.\n", dirname);
      bd->base_dir = strdup (dirname);
    }
  else
    {
      /* Try to get default directory */
      char *defaultbase = get_default_basedir ();
      if (defaultbase == NULL || dir_exists (defaultbase) == 0)
	{
	  /* Take current directory (as fallback) */
	  if (defaultbase != NULL)
	    {
	      outputf (INFO,
		       "[basedir] Default base directory '%s' does not exist, please create it first.\n",
		       defaultbase);
	      free (defaultbase);
	      defaultbase = NULL;
	    }
	  else
	    outputf (WARN,
		     "[basedir] Could not determine default base directory.\n");
	  outputf (INFO,
		   "[basedir] Using current directory as base directory.\n");
	  return bd;
	}
      /* Take just determined default directory */
      outputf (INFO, "[basedir] Using default base directory '%s'.\n",
	       defaultbase);
      bd->base_dir = defaultbase;
    }

  /* Fill basedir struct. */
  bd->cache_dir = dir_join (bd->base_dir, "cache");
  bd->metafile_dir = dir_join (bd->base_dir, "meta");
  bd->monfile_dir = dir_join (bd->base_dir, "monitor");
  return bd;
}

/*
 * Check if base directory is current directory.
 */
int
basedir_is_curdir (const basedirptr bd)
{
  if (bd == NULL)
    return 0;
  return (bd->base_dir == NULL ? 1 : 0);
}

/*
 * Check if base directory has a valid directory structure.
 */
int
basedir_is_prepared (const basedirptr bd)
{
  if (bd == NULL)
    return 0;
  /* Current directory is prepared by definition. */
  if (bd->base_dir == NULL)
    return 1;
  if (dir_exists (bd->cache_dir) == 0)
    return 0;
  if (dir_exists (bd->metafile_dir) == 0)
    return 0;
  if (dir_exists (bd->monfile_dir) == 0)
    return 0;
  return 1;
}

/*
 * Create directory if it doesn't already exists.
 */
static int
dir_safe_create (const char *dirname)
{
  if (dir_exists (dirname) == 0)
    {
      outputf (INFO, "[basedir] Directory '%s' does not exist, creating.\n",
	       dirname);
#ifndef _WIN32
      if (mkdir (dirname, 0755) != 0)
#else
      if (mkdir (dirname) != 0)
#endif
	{
	  outputf (ERROR, "[basedir] Could not create directory '%s'.\n",
		   dirname);
	  return RET_ERROR;
	}
    }
  return RET_OK;
}

/*
 * Create directory structure in base directory.
 */
int
basedir_prepare (const basedirptr bd)
{
  if (bd == NULL || bd->cache_dir == NULL || bd->metafile_dir == NULL
      || bd->monfile_dir == NULL)
    return RET_ERROR;
  if (dir_safe_create (bd->cache_dir) != RET_OK)
    return RET_ERROR;
  if (dir_safe_create (bd->metafile_dir) != RET_OK)
    return RET_ERROR;
  if (dir_safe_create (bd->monfile_dir) != RET_OK)
    return RET_ERROR;
  return RET_OK;
}

char *
basedir_buildpath_monfile (const basedirptr bd, const char *filename)
{
  if (bd == NULL || bd->monfile_dir == NULL)
    return strdup (filename);
  return dir_join (bd->monfile_dir, filename);
}

char *
basedir_buildpath_metafile (const basedirptr bd, const char *filename)
{
  if (bd == NULL || bd->metafile_dir == NULL)
    return strdup (filename);
  return dir_join (bd->metafile_dir, filename);
}

char *
basedir_buildpath_cache (const basedirptr bd, const char *filename)
{
  if (bd == NULL || bd->cache_dir == NULL)
    return strdup (filename);
  return dir_join (bd->cache_dir, filename);
}

xmlListPtr
basedir_get_all_monfiles (const basedirptr bd, xmlListPtr list)
{
  DIR *dir = NULL;
  struct dirent *entry = NULL;

  if (bd == NULL || bd->monfile_dir == NULL)
    return NULL;

  /* Open monitor file directory for reading. */
  dir = opendir (bd->monfile_dir);
  if (dir == NULL)
    {
      outputf (ERROR,
	       "[basedir] Could not open monitor file directory '%s' (%s)\n",
	       bd->monfile_dir, strerror (errno));
      return NULL;
    }

  /* Check each entry for file property. */
  outputf (DEBUG, "[basedir] Processing monitor file directory '%s'.\n",
	   bd->monfile_dir);
  entry = readdir (dir);
  while (entry != NULL)
    {
      char *fullpath = dir_join (bd->monfile_dir, entry->d_name);
      if (file_exists (fullpath) != 0)
	{
	  outputf (DEBUG, "[basedir] Adding monitor file '%s'.\n",
		   entry->d_name);
	  xmlListPushBack (list, strdup (entry->d_name));
	}
      else
	outputf (DEBUG, "[basedir] Ignoring directory entry '%s'.\n",
		 entry->d_name);
      free (fullpath);
      entry = readdir (dir);
    }
  closedir (dir);
  return list;
}

void
basedir_close (basedirptr bd)
{
  if (bd == NULL)
    return;
  if (bd->base_dir != NULL)
    free (bd->base_dir);
  if (bd->cache_dir != NULL)
    free (bd->cache_dir);
  if (bd->metafile_dir != NULL)
    free (bd->metafile_dir);
  if (bd->monfile_dir != NULL)
    free (bd->monfile_dir);
  xmlSafeFree (bd);
}
