#ifndef _QUERY_H_
#define _QUERY_H_

#include <stdio.h>

#define CDS_MIRROR "cds"

/* vizier catalogs to search */
enum {
	QUERY_UCAC2,
    QUERY_GSC23,
	QUERY_USNOB,
	QUERY_GSC_ACT,
	QUERY_TYCHO2,
    QUERY_UCAC4,
    QUERY_APASS,
    QUERY_GAIA,
    QUERY_HIP,
	QUERY_CATALOGS,
};

/* vizier cat name and search mag designation */
#define CAT_QUERY_NAMES { {"ucac2", "UCmag", "*"}, \
                          {"gsc2.3", "Vmag", "*"}, \
                          {"usnob", "B1mag", "*"}, \
                          {"gsc-act", "Pmag",  "*"}, \
                          {"tycho2", "VTmag", "*"}, \
                          {"ucac4", "Vmag", "*"}, \
                          {"apass", "Vmag", "*"}, \
                          {"gaia", "Gmag", "DR3Name, _RAJ2000, _DEJ2000, Gmag, e_Gmag, BPmag, e_BPmag, RPmag, e_RPmag, BP-RP, VarFlag, pmRA, pmDE"}, \
                          {"hip", "Vmag", "*"} }

int make_cat_rcp(char *obj, unsigned int catalog, FILE *outf) ;

#endif
