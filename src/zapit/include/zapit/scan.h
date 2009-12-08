/*
 *  $Id: scan.h,v 1.26 2003/03/14 07:31:50 obi Exp $
 */

#ifndef __scan_h__
#define __scan_h__

#include <linux/dvb/frontend.h>

#include <inttypes.h>

#include <map>
#include <string>

#include "bouquets.h"

extern CBouquetManager* scanBouquetManager;

char * getFrontendName();

#endif /* __scan_h__ */
