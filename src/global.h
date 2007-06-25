/* $Id$ */
#ifndef __WC_GLOBAL_H__
#define __WC_GLOBAL_H__

#define xmlSafeFree(v)	if ((v) != NULL) xmlFree((v)); (v) = NULL;

enum retvalue
{
  RET_OK = 0,
  RET_EOF = 1,
  RET_ERROR = -1,
  RET_WARNING = -2
};

enum outputlvl
{
  ERROR = -99,
  WARN = 0,
  NOTICE = 1,
  INFO = 2,
  DEBUG = 3
};

void outputf (int lvl, const char *fmt, ...);

#endif /* __WC_GLOBAL_H__ */
