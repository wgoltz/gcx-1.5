/*==================================================================
** NAME         :decode_c.c
** TYPE         :char *
** DESCRIPTION  :decodes GSC records (c[12] to record) Vers.2
** INPUT        :
** OUTPUT       :returns GSC rec. in string format
** AUTHOR       :a.p.martinez
** DATE         :12/92;
** DATE         :04/96: Removed sprintf -- slows down by a actor of 20 !!!
*=================================================================*/
/* ------ Encoding Version #2 : length of scaled data -----------------

	Data		bits

	spare		 1
	ID		14
	scaled RA	22
	scaled DEC	19
	pos.err.	 9
	magnitude	11
	mag.error	 7
	mag.band	 4      (Vers.1: 3)
	class.		 3	(Vers.1: 1)
	plate code	 4
	multip.code	 1	F=0,T=1
	
   ------ coding a record in c[12] --------------------------------- 

	c[0] = id >> 7;
	c[1] = (id << 1) | (da  >> 21);
	c[2] = da >> 13;
	c[3] = da >> 5;
	c[4] = (da << 3) | ((dd >> 16) & 7);
	c[5] = dd >> 8;
	c[6] = dd;
	c[7] = dp >> 1;
	c[8] = (dp << 7) | (dm & (255 >> 1));
	c[9] = mag >> 3;	
	c[10] = (mag << 5) | (ba << 1) | mul;
	c[11] = pl | (cl << 4);
-----------------------------------------------------------------------*/

#include <gsc.h>

char *decode_c(c,h,r)
	unsigned char *c;
	HEADER *h;
	GSCREC *r;
{
//	static char record[80];
        int id;
        INT da,dd,dp,mag,dm,pl,mul;
	int ba;
	char *p;

	id = (c[0] & 127);
	id <<= 7;
	id |= (c[1] >> 1);
	r->id = id;

	da = ((c[1]&1)<< 8) | c[2];
	da <<= 8;
	da |= c[3];
	da <<= 8;
	da |= c[4];
	da >>= 3;

	dd = c[4] & 7;
	dd <<= 8;
	dd |= c[5];
	dd <<= 8;
	dd |= c[6];

	dp = c[7];
	dp <<= 1;
	dp |= c[8] >> 7;

	mag = c[9];
	mag <<= 3;
	mag |= c[10] >> 5;

	dm = c[8] & 127;

	ba = (c[10] >> 1) & 15;
	r->mb = band[ba];	

	r->cl = (c[11] >> 4) & 7;

	pl = c[11] & 15;
	p = h->list;
	strncpy(r->plate,p+(5*pl)+1,4);
	r->plate[4] = '\0';

	p = h->list+5*h->npl;
	p += 9*pl ;
	if (isdigit(p[1])) r->epoch = atof(p+1) - 2000.0; 
	else r->epoch = 0;

	mul = c[10] & 1;
	if(mul == 0) r->mu = 'F';
	if(mul == 1) r->mu = 'T';

	r->ra = (double)da/h->scale_ra +h->amin;
	if (r->ra < 0.) r->ra += 360.;
	if (r->ra >= 360.) r->ra -= 360.;
	r->dec = (double)dd/h->scale_dec +h->dmin;
	r->poserr = (double)dp/h->scale_pos;
	r->m = (double)mag/h->scale_mag +h->magoff;
	r->merr = (double)dm/h->scale_mag;
	r->reg = h->region;

	/* sprintf(record,
	"%05d%05d %09.5lf %+09.5lf %5.1f %5.2f %4.2f %2d %1d %s %1c",
	 h->region,r->id,r->ra,r->dec,r->poserr,r->m,r->merr,r->mb,r->cl,
	 r->plate,r->mu);
	*/

	return((char *)0);
}

