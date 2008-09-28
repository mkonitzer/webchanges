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
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
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
  char *joined = malloc (strlen (cur) + 1 + strlen (add) + 1);
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
    return RET_ERROR;
  if (S_ISDIR (st.st_mode) == 0)
    return RET_ERROR;
  return RET_OK;
}

/*
 * Checks if the specified file exists.
 */
static int
file_exists (const char *filename)
{
  struct stat st;
  if (stat (filename, &st) != 0)
    return RET_ERROR;
  if (S_ISREG (st.st_mode) == 0)
    return RET_ERROR;
  return RET_OK;
}

/*
 * Get the default base directory where monitor/meta/cache files reside.
 */
static char *
get_default_basedir (void)
{
  char *basedir;
#ifdef _WIN32
  const char *appdatadir = NULL;
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
  /* (1) Take the user-specified directory */
  if (dirname != NULL)
    {
      if (dir_exists (dirname) != RET_OK)
	{
	  outputf (ERROR, "[basedir] Base directory %s does not exist.\n",
		   dirname);
	  xmlFree (bd);
	  return NULL;
	}
      outputf (INFO, "[basedir] Using base directory %s.\n", dirname);
      bd->base_dir = strdup (dirname);
    }
  else
    {
      /* (2) Try to get default directory or */
      bd->base_dir = get_default_basedir ();
      if (bd->base_dir != NULL || dir_exists (bd->base_dir) != RET_OK)
	{
	  /* (3) Use current directory (fallback option) */
	  if (bd->base_dir != NULL)
	    outputf (INFO,
		     "[basedir] Default base directory %s does not exist.\n",
		     bd->base_dir);
	  else
	    outputf (INFO,
		     "[basedir] Could not determine default base directory.\n");
	  outputf (INFO, "[basedir] Falling back to current directory.\n");
	  xmlSafeFree (bd->base_dir);
	  bd->base_dir = NULL;
	  bd->cache_dir = NULL;
	  bd->metafile_dir = NULL;
	  bd->monfile_dir = NULL;
	  return bd;
	}
      outputf (INFO, "[basedir] Using default base directory %s.\n",
	       bd->base_dir);
      bd->base_dir = strdup (bd->base_dir);
    }
  /* Fill basedir struct. */
  bd->cache_dir = dir_join (bd->base_dir, "cache");
  bd->metafile_dir = dir_join (bd->base_dir, "meta");
  bd->monfile_dir = dir_join (bd->base_dir, "monitor");
  return bd;
}

/*
 * Check if the specified directory has a valid base directory structure.
 */
int
basedir_usable (const basedirptr basedir)
{
  /* TODO: implement me! */
  return RET_ERROR;
}

char *
basedir_get_monfile (const basedirptr bd, const char *filename)
{
  /* Search current directory first */
  if (file_exists (filename))
    return strdup (filename);
  /* Search base directory then */
  if (bd->monfile_dir == NULL)
    return NULL;
  return dir_join (strdup (bd->monfile_dir), filename);
}

char *
basedir_get_metafile (const basedirptr bd, const char *filename)
{
  /* Search current directory first */
  if (file_exists (filename))
    return strdup (filename);
  /* Search base directory then */
  if (bd->metafile_dir == NULL)
    return NULL;
  return dir_join (strdup (bd->metafile_dir), filename);
}

char *
basedir_get_cache (const basedirptr bd, const char *filename)
{
  /* Search current directory first */
  if (file_exists (filename))
    return strdup (filename);
  /* Search base directory then */
  if (bd->cache_dir == NULL)
    return NULL;
  return dir_join (strdup (bd->cache_dir), filename);
}

void
basedir_safefree (char *filename)
{
  if (filename != NULL)
    free (filename);
}
