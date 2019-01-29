/**************************************************************

matstack.h (C-Munipack project)
Copyright (C) 2003 David Motl, dmotl@volny.cz

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

**************************************************************/

#ifndef CMPACK_MATSTACK_H
#define CMPACK_MATSTACK_H

/* Transformation */
struct _CmpackMatchTrafo
{
  int    matched;						/* Kolikrat stejna serie */
  int    *id1;           				/* Cisla hvezd v serii 1 */
  int    *id2;           				/* Cisla hvezd v serii 2 */
  double m[9];							/* Tranformation matrix */
  double sumsq;							/* Sum of squares */
  int    nstar, mstar;					/* Polygon vertices, matched stars */
  struct _CmpackMatchTrafo *next;		/* dalsi hvezda nebo NULL */
};
typedef struct _CmpackMatchTrafo CmpackMatchTrafo;

/* Transformation stack */
struct _CmpackMatchStack
{
	CmpackMatchTrafo *first, *last;		
};
typedef struct _CmpackMatchStack CmpackMatchStack;

/* Initialize stack */
void StInit(CmpackMatchStack *lc);

/* Add a new member to a stack */
void StAdd(CmpackMatchStack *lc, const int *id1, const int *id2, int nstar, double sumsq, 
		   int mstar, const double *matrix);
       
/* Find the best member */
void StSelect(CmpackMatchStack *lc, int *n, double *sumsq, double *matrix);
       
/* Print content of the stack to debug output */
void StDump(CmpackConsole *con, CmpackMatchStack *lc);
       
/* Free allocated memory a initialize stack */
void StClear(CmpackMatchStack *lc);

#endif
