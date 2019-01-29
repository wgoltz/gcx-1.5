#ifndef _TYCHO2_H_
#define _TYCHO2_H_

/* size of a TYCHO2 record, without the trailing zero */
#define TYCRECSZ 207

int tycho2_search(double ra, double dec, double w, double h, 
		  char *buf, int sz, char *path);


#endif
