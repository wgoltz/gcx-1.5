/*==================================================================
** NAME         :prtgsc.c
** TYPE         :int 
** DESCRIPTION  :prints GSC record with format according to fmt
** INPUT        :
** OUTPUT       :stdout
** AUTHOR       :a.p.martinez
** DATE         :10/92
** DATE         :05/95  (fo) format RA / DEC changed to %09.5f %+09.5f
*=================================================================*/
#include <gsc.h>

static char *form2  = 
	"%05d%05d %09.5lf %+09.5lf %5.2f; %6.2f %3.0f";
static char *form3  = 
	"%05d%05d%s %s %5.2f; %6.2f %3.0f";
static char *form0  = 
	"%05d%05d %09.5lf %+09.5lf%6.1f%6.2f%5.2f%3d%2d%5s %c;%7.2f%4.0f";
static char *form10 = 
	"%05d%05d %09.5lf %+09.5lf%6.1f%6.2f%5.2f%3d%2d%s %c;%7.2f%4.0f";
static char *form30 = 
	"%05d%05d %09.5lf %+09.5lf%6.1f%6.2f%5.2f%3d%2d%5s%s %c;%7.2f%4.0f";
static char *form1  = 
	"%05d%05d%s %s%6.1f%6.2f%5.2f%3d%2d%5s %c;%7.2f%4.0f";
static char *form11 = 
	"%05d%05d%s %s%6.1f%6.2f%5.2f%3d%2d%s %c;%7.2f%4.0f";
static char *form31 = 
	"%05d%05d%s %s%6.1f%6.2f%5.2f%3d%2d%5s%s %c;%7.2f%4.0f";
static char *formol = /* To get original GSC format */
	"%05d%05d %09.5lf %+09.5lf %5.1f %5.2f %4.2f %2d %1d %s %1c";

static char *entete="\
GSC-id      ra   (2000)   dec  pos-e  mag mag-e  b c   pl mu     d'  pa";
static char *entete10="\
GSC-id      ra   (2000)   dec  pos-e  mag mag-e  b c   epoch  mu     d'  pa";
static char *entete30="\
GSC-id      ra   (2000)   dec  pos-e  mag mag-e  b c   pl   epoch  mu     d'  pa";
static char *entete1="\
GSC-id        ra   (2000)   dec    pos-e  mag mag-e  b c   pl mu     d'  pa";
static char *entete11="\
GSC-id        ra   (2000)   dec    pos-e  mag mag-e  b c   epoch  mu     d'  pa";
static char *entete31="\
GSC-id        ra   (2000)   dec    pos-e  mag mag-e  b c   pl   epoch  mu     d'  pa";
static char *entete2="\
GSC-id      ra   (2000)   dec    mag      d'  pa";
static char *entete3="\
GSC-id        ra   (2000)   dec      mag      d'  pa";

static char line[256];

char *gsc2a(fmt,iepoch,r)
  int fmt,iepoch;
  GSCREC *r;
{
  char sa1[20],sd1[20], epa[12];
  double rar,der, ep;
  int end = 31;
  int nodist;
  char *p;

	nodist = fmt&0x100;
	if (iepoch && r) {
	    if (r->epoch) ep = 2000.+r->epoch, sprintf(epa, "%9.3f", ep);
	    else strcpy(epa, " ........");
	}
	*line = 0;

	switch(fmt&0xf) {
	case 3:
	    if (!r) return(entete3);
		rar = r->ra;
		der = r->dec;
		rar /= 15.0;
		dtos(&rar,sa1,2); sa1[0]=' ';
		dtos(&der,sd1,1);
		end += 4;
		sprintf(line,form3,r->reg,r->id,sa1,sd1,
		    r->m,r->dist,r->posang);
		break;
	case 2:
	    if (!r) return(entete2);
		rar = r->ra;
		der = r->dec;
		sprintf(line,form2,r->reg,r->id,r->ra,r->dec,
		r->m,r->dist,r->posang);
		break;

	case 1:
	    if (!r) return(iepoch==0 ? entete1 : 
	       iepoch == 1 ? entete11 : entete31);
		rar = r->ra;
		der = r->dec;
		rar /= 15.0;
		dtos(&rar,sa1,2); sa1[0]=' ';
		dtos(&der,sd1,1);
		end += 4;
	   if (iepoch == 0)
		sprintf(line,form1,r->reg,r->id,sa1,sd1,
		r->poserr,r->m,r->merr,r->mb,r->cl,
		r->plate,r->mu,r->dist,r->posang);
	   else if (iepoch == 1)
		sprintf(line,form11,r->reg,r->id,sa1,sd1,
		r->poserr,r->m,r->merr,r->mb,r->cl,
		epa,r->mu,r->dist,r->posang);
	   else 
		sprintf(line,form31,r->reg,r->id,sa1,sd1,
		r->poserr,r->m,r->merr,r->mb,r->cl,
		r->plate,epa,r->mu,r->dist,r->posang);
		break;
	case 0:
	  if (!r) return(iepoch==0 ? entete : 
	      iepoch==1 ? entete10 : entete30 );
		rar = r->ra;
		der = r->dec;
	   if (iepoch == 0)
		sprintf(line,form0,r->reg,r->id,r->ra,r->dec,
		r->poserr,r->m,r->merr,r->mb,r->cl,
		r->plate,r->mu,r->dist,r->posang);
	   else if (iepoch == 1)
		sprintf(line,form10,r->reg,r->id,r->ra,r->dec,
		r->poserr,r->m,r->merr,r->mb,r->cl,
		epa,r->mu,r->dist,r->posang);
	   else
		sprintf(line,form30,r->reg,r->id,r->ra,r->dec,
		r->poserr,r->m,r->merr,r->mb,r->cl,
		r->plate,epa,r->mu,r->dist,r->posang);
		break;
	 default:	/* Original GSC */
	   	if (r) sprintf(line,formol, r->reg,r->id,r->ra,r->dec,
		r->poserr,r->m,r->merr,r->mb,r->cl,r->plate,r->mu);
		return(line) ;
	}
	if (!r->mu) line[end]=0;
	if (nodist) {
	    p = strchr(line, ';');
	    if (p) *++p = 0;
	}
	return(line);
}

void prtgsc(fmt,iepoch,r)
  int fmt,iepoch;
  GSCREC *r;
{
	puts(gsc2a(fmt,iepoch,r));
}
