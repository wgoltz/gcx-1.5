/*==================================================================
** NAME         :dtos.c
** TYPE         :void
** DESCRIPTION  :transforms a double into a string (base 60)
**              :
** INPUT        :double *deci [,int prec]
**              :
** OUTPUT       :char *string
**              :
** AUTHOR       :apm
** DATE         :06/91; 09/91;
** LIB          :pm.a  - use cco, libad
*=================================================================*/

#include <math.h>
#include <stdio.h>

void dtos(deci,string,prec)
	double *deci;
	char *string;
	int prec;
{
	char sign;
	int h,m;
	double dd,s,md,fabs();

	sign='+';
	if(*deci < 0.0) sign='-';
	dd= fabs(*deci)+0.0000000001;
	h=(int)dd;
	md=(dd-h)*60.;
	m=(int)md;
	s=(md-m)*60.;
	if(prec == 3)
	sprintf(string,"%c%02d %02d %06.3f",sign,h,m,s);
	else if(prec == 1)
	sprintf(string,"%c%02d %02d %04.1f",sign,h,m,s);
	else if(prec == 0)
	sprintf(string,"%c%02d %02d %02.0f",sign,h,m,s);
	else if(prec == -1)
	sprintf(string,"%c%02d %04.1f",sign,h,md);
	else
	sprintf(string,"%c%02d %02d %05.2f",sign,h,m,s);
	return;
}
