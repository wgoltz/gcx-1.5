/*==================================================================
** NAME         :find_reg.c
** TYPE         :int
** DESCRIPTION  :find region-path according to reg-number
** INPUT        :
** OUTPUT       :
** AUTHOR       :a.p.martinez
** DATE         :10/92;
** LIB          :
*=================================================================*/

#include <stdio.h>
#include <stdlib.h>
#define ITEMS(a)	sizeof(a)/sizeof(a[0])

static int range[24] = {1,594,1178,1729,2259,2781,3246,3652,4014,4294,4492,
	4615,4663,5260,5838,6412,6989,7523,8022,8464,8840,9134,9346,9490};
static char *subdir[] = {"n0000","n0730","n1500","n2230","n3000","n3730",
	"n4500","n5230","n6000","n6730","n7500","n8230","s0000",
	"s0730","s1500","s2230","s3000","s3730","s4500","s5230",
	"s6000","s6730","s7500","s8230"};

int find_reg (region, nreg, path)
  char *region, *path;
  int nreg;
{
  int i;

	for (i = ITEMS(range); --i >= 0; )
	    if (nreg >= range[i]) break;

	sprintf(region,"%s/%s/%04d.gsc",
	    path, (i>=0 ? subdir[i] : "-----"), nreg);
	return(1);
}
