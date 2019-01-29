/*==================================================================
** NAME         :dispos.c
** TYPE         :double
** DESCRIPTION  :dispos computes distance and position angle
**              :solving a spherical triangle (no approximations)
** INPUT        :coo in decimal degrees
** OUTPUT       :dist in arcmin, returns phi in degrees (East of North)
** AUTHOR       :a.p.martinez
** DATE         :91;
** LIB          :
*=================================================================*/
#include <stdio.h>
#include <math.h>

double dispos(dra0,decd0,dra,decd,dist)
	double *dra, *dra0, *decd, *decd0, *dist;
{
	double alf,alf0,del,del0,phi;
	double sd,sd0,cd,cd0,cosda,cosd,sind,sinpa,cospa;
	double radian=180./M_PI;

/* coo transformed in radiants */

	alf = *dra / radian ;
	alf0= *dra0 / radian ;
	del = *decd / radian;
	del0= *decd0 / radian;

	sd0=sin(del0);
	sd =sin(del);
	cd0=cos(del0);
	cd =cos(del);
	cosda=cos(alf-alf0);
        cosd=sd0*sd+cd0*cd*cosda;
	*dist=acos(cosd);
	phi=0.0;
	if(*dist > 0.0000004)
	{
	   sind=sin(*dist);
	   cospa=(sd*cd0 - cd*sd0*cosda)/sind;
	   if(cospa>1.0)cospa=1.0;
	   sinpa=cd*sin(alf-alf0)/sind;
	   phi=acos(cospa)*radian;
	   if(sinpa < 0.0)phi = 360.0-phi;
	}
	*dist *=radian;
	*dist *=60.0;
	if(*decd0 == 90.) phi = 180.0;
	if(*decd0 == -90.) phi = 0.0;
	return(phi);
}
