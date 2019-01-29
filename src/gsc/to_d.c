/*==================================================================
** NAME         :to_d.c
** TYPE         :int
** DESCRIPTION  :tranforms coordinates from string format (free)
**              :to decimal format
** INPUT        :RA +/- DEC
** OUTPUT       :double ra dec 
** AUTHOR       :apm
** DATE         :06/91; 09/91;
** 		:04/96: FO: Modified, transform RA into degrees
**		 if at least one blank in RA field
** LIB          :use cca to compile
*=================================================================*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/*==================================================================
** NAME         :tod.c
** TYPE         :void
** DESCRIPTION  :transforms a [signed]sexagesimal string to decimal
**              :accepts free input format
** INPUT        :char *string
** OUTPUT       :double *decimal
** AUTHOR       :apm
** DATE         :06/91; 09/91
*=================================================================*/

void tod(string,deci)
	double *deci;
	char *string;
{
	int sign;
	double dd,s=0.,h=0.,m=0.; 
	char minus='-' , *ps;

	if(*string == '-' || *string == '+')
	{
	  ps=string;
	  while(*(++ps) == ' ');
	  if(ps-1 != string)
	  {  *(ps-1) = *string;
	     *string = ' ';  }
	}
	sign=1;
	if(strchr(string,minus))sign= -1;
	sscanf(string,"%lf%lf%lf",&h,&m,&s);
	dd=(s/60.+ m)/60.;
	if(sign < 0) *deci= h-dd;
	else         *deci= h+dd; 
	return;
}

int to_d(str,alphad,decd)
	char *str;
	double *alphad, *decd;
{
	char *ps, *pp=NULL;
	char x, *p="+", *m="-";

	while (isspace(*str)) str++;
	if (ispunct(*str)) return(-1);
	if( (ps=strpbrk(str,p)) ) pp=ps;
	else if( (ps=strpbrk(str,m)) ) pp=ps;
	if(!pp)  return(-1);
	if (pp == str) return(-1); /* delta without alpha */
	while(isspace(pp[-1])) pp--;

	x = *pp, *pp = '\0';
	tod(str,alphad);  
	if (strpbrk(str, " :")) *alphad *= 15.0 ;
	*pp = x;
	tod(pp,decd);  
	return(1);
}
