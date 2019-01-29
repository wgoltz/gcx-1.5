/*==================================================================
** NAME         :get_header.c
** TYPE         :char *
** DESCRIPTION  :get header of coded GSC region
** INPUT        :
** OUTPUT       :returns header in string format
** AUTHOR       :a.p.martinez
** DATE         :10/92;
** LIB          :
*=================================================================*/
#include <gsc.h>
#include <stdio.h>
#include <unistd.h>

char *get_header(fp,h)
  int fp;
  HEADER *h;
{
  static char ww[512];
  char *w;
  int lheader;

	w = ww;
	memset( h, 0, sizeof(HEADER));
	memset(ww, 0, sizeof(ww));

	if (read(fp,w,4) != 4) return((char *)0);

	lheader = atoi(w);
	w += 4;
	if (read(fp,w,lheader-4) > 0)
		w[lheader] = '\0';
	else
		*w = '\0';

	h->len = lheader;
	h->vers = atoi(w);
	while(*++w != ' ');
	h->region = atoi(w);
	while(*++w != ' ');
	h->nobj = atoi(w);
	while(*++w != ' ');
	h->amin = atof(w);
	while(*++w != ' ');
	h->amax = atof(w);
	while(*++w != ' ');
	h->dmin = atof(w);
	while(*++w != ' ');
	h->dmax = atof(w);
	while(*++w != ' ');
	h->magoff = atof(w);
	while(*++w != ' ');
	h->scale_ra = atof(w);
	while(*++w != ' ');
	h->scale_dec = atof(w);
	while(*++w != ' ');
	h->scale_pos = atof(w);
	while(*++w != ' ');
	h->scale_mag = atof(w);
	while(*++w != ' ');
	h->npl = atoi(w);
	while(*++w != ' ');
	h->list = w;

	return(ww);
}

