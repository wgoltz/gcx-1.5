/**************************************************************

matfun.h (C-Munipack project)
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

#ifndef CMPACK_MATCH_H
#define CMPACK_MATCH_H

#include "catalogs.h"

/** \brief Error codes */
typedef enum _CmpackError
{
	CMPACK_ERR_OK				= 0,		/**< Operation finished successfully */
	CMPACK_ERR_MEMORY			= 1001,		/**< Insufficient memory */
	CMPACK_ERR_KEY_NOT_FOUND    = 1002,		/**< Key not found */
	CMPACK_ERR_COL_NOT_FOUND	= 1003,		/**< Column not found */
	CMPACK_ERR_ROW_NOT_FOUND	= 1004,		/**< Row not found */
	CMPACK_ERR_AP_NOT_FOUND		= 1005,		/**< Aperture not found */
	CMPACK_ERR_READ_ONLY		= 1006,		/**< File is open in read-only mode */
	CMPACK_ERR_CLOSED_FILE      = 1007,		/**< Operation not allowed on closed file */
	CMPACK_ERR_OPEN_ERROR		= 1008,		/**< Error while opening file */
	CMPACK_ERR_READ_ERROR		= 1009,		/**< Error while reading file */
	CMPACK_ERR_WRITE_ERROR		= 1010,		/**< Error while writing file */
	CMPACK_ERR_UNKNOWN_FORMAT	= 1011,		/**< Unknown format of source file */
	CMPACK_ERR_BUFFER_TOO_SMALL = 1012,		/**< Buffer is too small */
	CMPACK_ERR_INVALID_CONTEXT  = 1013,     /**< Invalid context */
	CMPACK_ERR_OUT_OF_RANGE     = 1014,     /**< Index is out of range */
	CMPACK_ERR_UNDEF_VALUE      = 1015,     /**< Undefined value */
	CMPACK_ERR_MAG_NOT_FOUND    = 1016,		/**< Measurement not found */
	CMPACK_ERR_STAR_NOT_FOUND	= 1017,		/**< Star not found */
	CMPACK_ERR_NOT_IMPLEMENTED  = 1018,		/**< Unsupported operation */
	CMPACK_ERR_INCOMPATIBLE_FRM = 1019,		/**< Incompatible frame */
	CMPACK_ERR_FRAME_NOT_FOUND  = 1020,		/**< Frame not found */
	CMPACK_ERR_INCOMPATIBLE_TYPE= 1021,		/**< Incompatible type */
	CMPACK_ERR_INVALID_SIZE     = 1100,		/**< Invalid dimensions of image */
	CMPACK_ERR_INVALID_DATE     = 1101,		/**< Invalid date/time format */
	CMPACK_ERR_INVALID_PAR      = 1102,		/**< Invalid value of parameter */
	CMPACK_ERR_INVALID_RA		= 1103,		/**< Invalid RA format */
	CMPACK_ERR_INVALID_DEC		= 1104,		/**< Invalid DEC format */
	CMPACK_ERR_INVALID_EXPTIME	= 1105,		/**< Invalid exposure duration */
	CMPACK_ERR_INVALID_BITPIX   = 1106,     /**< Image image data format */
	CMPACK_ERR_INVALID_LON		= 1107,		/**< Invalid format of longitude */
	CMPACK_ERR_INVALID_LAT		= 1108,		/**< Invalid format of latitude */
	CMPACK_ERR_INVALID_JULDAT	= 1109,		/**< Invalid date/time of observation */
	CMPACK_ERR_CANT_OPEN_SRC	= 1200,		/**< Cannot open the source file */
	CMPACK_ERR_CANT_OPEN_OUT	= 1201,		/**< Cannot open the destination file */
	CMPACK_ERR_CANT_OPEN_BIAS	= 1202,		/**< Bias frame not found */
	CMPACK_ERR_CANT_OPEN_DARK	= 1203,		/**< Dark frame not found */
	CMPACK_ERR_CANT_OPEN_FLAT	= 1204,		/**< Flat frame not found */
	CMPACK_ERR_CANT_OPEN_REF	= 1205,		/**< Reference file not found */
	CMPACK_ERR_CANT_OPEN_PHT	= 1206,		/**< Cannot open photometry file */
	CMPACK_ERR_DIFF_SIZE_SRC	= 1300,		/**< Input frames are not compatible (different sizes) */
	CMPACK_ERR_DIFF_SIZE_BIAS   = 1301,		/**< Bias frame is not compatible (different sizes) */
	CMPACK_ERR_DIFF_SIZE_DARK  	= 1302,		/**< Dimensions of dark-frame and scientific image are different */
	CMPACK_ERR_DIFF_SIZE_FLAT   = 1303,		/**< Dimensions of flat-frame and scientific image are different */ 
	CMPACK_ERR_DIFF_BITPIX_SRC	= 1304,		/**< Input frames are not compatible (different image data type) */
	CMPACK_ERR_NO_INPUT_FILES	= 1400,		/**< No input files */
	CMPACK_ERR_NO_BIAS_FRAME	= 1401,		/**< Missing bias frame */
	CMPACK_ERR_NO_DARK_FRAME	= 1402,		/**< Missing dark frame */
	CMPACK_ERR_NO_FLAT_FRAME	= 1403,		/**< Missing flat frame */
	CMPACK_ERR_NO_OBS_COORDS	= 1404,		/**< Missing observer's coordinates */
	CMPACK_ERR_NO_OBJ_COORDS	= 1405,		/**< Missing object's coordinates */
	CMPACK_ERR_NO_OUTPUT_FILE	= 1406,		/**< Missing name of output frame */
	CMPACK_ERR_NO_REF_FILE		= 1407,		/**< Missing name of reference frame */
	CMPACK_ERR_MEAN_ZERO    	= 1500,		/**< Mean value of flat frame is zero (can't divide by zero) */
	CMPACK_ERR_REF_NOT_FOUND	= 1501,		/**< Refererence star was not found */
	CMPACK_ERR_FEW_POINTS_REF	= 1502,		/**< Too few stars in the reference file */
	CMPACK_ERR_FEW_POINTS_SRC	= 1503,		/**< Too few stars in the source file */
	CMPACK_ERR_MATCH_NOT_FOUND	= 1504		/**< Coincidences not found  */
} CmpackError;

/**
	\brief Transformation matrix
	\details A CmpackMatrix holds an affine transformation, such as a scale, rotation, 
	shear, or a combination of those. The transformation of a point (x, y) is given by:
    x_new = xx * x + xy * y + x0;
    y_new = yx * x + yy * y + y0;
*/
typedef struct _CmpackMatrix {
    double xx;				/**< xx component of the affine transformation */
	double yx;				/**< yx component of the affine transformation */
    double xy;				/**< xy component of the affine transformation */
	double yy;				/**< yy component of the affine transformation */
    double x0;				/**< x translation component of the affine transformation */
	double y0;				/**< y translation component of the affine transformation */
} CmpackMatrix;

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

typedef enum _CmpackMatchMethod 
{
	CMPACK_MATCH_STANDARD,
	CMPACK_MATCH_AUTO,
	CMPACK_MATCH_SPARSE_FIELDS
} CmpackMatchMethod;

struct _CmpackMatchObject
{
	int	i;						/**< Object's ordinal number */
	double r;					/**< Relative radius */
};

typedef struct _CmpackFrame
{
	int width, height;		/**< frame dimensions in pixels */
	int c;					/**< No. of stars in file */
	int n;					/**< No. of stars for matching */
	double *x, *y;			/**< Coordinates of stars */
	
	struct _CmpackMatchObject *idr;
	int *id;
} CmpackFrame;

/* Matching context */
struct _CmpackMatch
{
	/* Configuration parameters: */
	int nstar;                  /**< Number of vertices of polygons */
	int maxstar;                /**< Maximum number of stars used in Solve procedure */
	double clip;                /**< Clipping threshold in sigmas */
	CmpackMatchMethod method;	/**< Matching method */
	double maxoffset;			/**< Max. position offset for Simple method */
    int simple;                 /**< internal flag (use simple in FindTrafo) */

	/* Reference frame: */
    CmpackFrame reference_frame;

	/* Last frame */
	int matched_stars;			/**< Number of stars matched */
	double offset[2];			/**< Frame offset in pixels */
};
typedef struct _CmpackMatch CmpackMatch;

struct _CmpackMatchFrame
{
	/* Reference frame: */
    CmpackFrame *reference;

	/* Input frame: */
    CmpackFrame input;
	
	int *xref;					/**< Cross-reference id */
	
	/* Preallocated buffers: */
	double *k, *dev;

	/* Transformation stack: */
	CmpackMatchStack stack;		/**< Transformation stack */

	/* Result */
	int mstar;					/**< Number of matched stars */
	CmpackMatrix trafo;			/**< Transformation matrix */
};
typedef struct _CmpackMatchFrame CmpackMatchFrame;

void StInit(CmpackMatchStack *st);
void StAdd(CmpackMatchStack *st, const int *id1, const int *id2, int nstar, double sumsq, int mstar, const double *m);
void StDump(CmpackMatchStack *st);
void StSelect(CmpackMatchStack *st, int *mstar, double *sumsq, double *matrix);
void StClear(CmpackMatchStack *st);

int Simple(CmpackMatch *cfg, CmpackMatchFrame *lc);
int Solve(CmpackMatch *cfg, CmpackMatchFrame *lc);

void cmpack_match_destroy(CmpackMatch *cfg);
int cmpack_match_get_offset(CmpackMatch *cfg, double *offset_x, double *offset_y);
int cmpack_match(CmpackMatch *cfg, int width, int height, GSList *input);
CmpackMatch *cmpack_match_init(int width, int height, GSList *reference);

void cmpack_matrix_transform_point(const CmpackMatrix *matrix, double *x, double *y);
void cmpack_matrix_init(CmpackMatrix *matrix, double xx, double yx, double xy, double yy, double x0, double y0);

#endif
