/* $Id$ */
#ifndef __WC_VPAIR_H__
#define __WC_VPAIR_H__

#include <libxml/xmlstring.h>

typedef struct _vpair vpair;
typedef vpair *vpairptr;

/* vpair functions */
vpairptr vpair_open (const xmlChar * url);
int vpair_parse (vpairptr vp);
int vpair_download (vpairptr vp);
int vpair_remove (vpairptr vp);
void vpair_close (vpairptr vp);
xmlChar *vpair_get_url (vpairptr vp);
xmlChar *vpair_get_cache (vpairptr vp);
xmlDocPtr vpair_get_old_doc (vpairptr vp);
xmlDocPtr vpair_get_cur_doc (vpairptr vp);

#endif /* __WC_VPAIR_H__ */
