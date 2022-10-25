//#include <opencv2/core/core.hpp>
//#include <opencv2/imgproc/imgproc.hpp>
//#include <opencv2/video/tracking.hpp>
#include <math.h>
#include <vector>

#include "ccd/ccd.h"
#include "warpaffine.h"
#include "sourcesdraw.h"
#include "reduce.h"

#include <QPainter>
#include <QBitmap>
#include <QGuiApplication>

using namespace cv;

bool do_line(const QImage & maskImage, int h, int &start, int &end) {
    bool found_visible = false;
    int w = 0;
    start = 0;
    end = maskImage.width() - 1;
    while (w < maskImage.width()) {
        if (maskImage.pixel(w, h) == Qt::color1) {// start of visible part
            start = w;
            w++;
            while (w < maskImage.width()) {
                if (maskImage.pixel(w, h) != Qt::color1) { // end of visible part
                    end = w;
                    break; // finished this line
                }
                w++;
            }
            found_visible = true;
            break;

        } else
            w++;
    }
    return found_visible;
}

QBitmap *new_alignment_mask(int w, int h)
{
    QBitmap *mask = new QBitmap(w, h);
    QPainter painter(mask);
    painter.fillRect(0, 0, w - 1, h - 1, Qt::color1);
    return mask;
}

extern "C" {

void *start_QGuiApplication()
{
    int i = 1;
    char *name[] = {"gcx"};
    return new QGuiApplication(i, name);
}


struct visible_bounds *new_visible_bounds(int height)
{
    struct visible_bounds *bounds = (struct visible_bounds*) malloc(sizeof(struct visible_bounds) + height * sizeof(struct line_bounds));
    bounds->row = (struct line_bounds *) (bounds + 1);
    return bounds;
}

/*
 * convert fr->alignment_mask to visible_bounds
 * malloc new visible_bounds if bounds = NULL
*/
struct visible_bounds *frame_bounds(struct ccd_frame *fr, struct visible_bounds *bounds, int full_frame)
{
    if (! fr->alignment_mask)
        fr->alignment_mask = new_alignment_mask(fr->w, fr->h);

    QImage maskImage = ((QBitmap *) fr->alignment_mask)->toImage();

    if (! bounds)
        bounds = new_visible_bounds(maskImage.height());

    if (full_frame) {
        bounds->first_row = 0;
        int linestart = 0, lineend = maskImage.width();
        int i = 0;
        while (i < maskImage.height()) {
            bounds->row[i].first = linestart;
            bounds->row[i].last = lineend;
            i++;
        }
        bounds->num_rows = maskImage.height();
    } else {
        bounds->num_rows = 0;

        int h = 0;
        while (h < maskImage.height()) {
            int linestart, lineend;
            if (do_line(maskImage, h, linestart, lineend)) {
                bounds->first_row = h;
                int i = 0;

                do {
                    bounds->row[i].first = linestart;
                    bounds->row[i].last = lineend;
                    h++;
                    i++;
                } while (h < maskImage.height() && do_line(maskImage, h, linestart, lineend));
                bounds->num_rows = i;
            } else {
                h++;
            }
        }
    }

    return bounds;
}

/*
 * AND alignment_masks of (unskipped) frames in imflist with fr->alignment_mask
*/
void AND_frame_bounds(struct image_file_list *imfl, struct ccd_frame *fr)
{
    GList *sl = imfl->imlist;
    if (! sl) return;

    QBitmap *mask;

    if (! fr->alignment_mask)
        mask = new_alignment_mask(fr->w, fr->h);
    else
        mask = (QBitmap *) fr->alignment_mask;

    QPainter painter(mask);
    painter.setCompositionMode(QPainter::RasterOp_SourceAndDestination);

    struct image_file *imf = (image_file *) sl->data;
    do {
        sl = sl->next;
        if (! sl) break;

        imf = (image_file *) sl->data;
        if (! (imf->flags & IMG_SKIP)) {
            QBitmap *frame = (QBitmap *) imf->fr->alignment_mask;
            if (! frame)
                frame = new_alignment_mask(imf->fr->w, imf->fr->h);
            painter.drawPixmap(0, 0, *frame);
        }
    } while(true);

    fr->alignment_mask = mask;
}


/*
 * create mask of visible pixels for fr aligned to align_fr
 * pointed to at fr->alignment_mask
 */
void make_alignment_mask(struct ccd_frame *fr, struct ccd_frame *align_fr, double dx, double dy, double ds, double dt)
{
    QPixmap a(align_fr->w, align_fr->h);
    QBitmap *mask;
    if (! align_fr->alignment_mask)
        mask = new_alignment_mask(align_fr->w, align_fr->h);
    else
        mask = (QBitmap *) align_fr->alignment_mask;

    QBitmap *frame;
    if (! fr->alignment_mask)
        frame = new_alignment_mask(fr->w, fr->h);
    else
        frame = (QBitmap *) fr->alignment_mask;

    // use qt paint functions to create mask

    double st, ct;
    sincos(dt, &st, &ct);
    st *= ds;
    ct *= ds;
    frame->transformed(QTransform(ct, st, -st, ct, dx, dy));

    QPainter painter(mask);

    painter.setPen(Qt::color1);
    painter.drawPixmap(0, 0, fr->w, fr->h, *frame);

    fr->alignment_mask = mask;
}


void free_alignment_mask(struct ccd_frame *fr)
{
    QBitmap *mask = (QBitmap *) fr->alignment_mask;
    delete mask;
    fr->alignment_mask = NULL;
}


/*
 * use OpenCV to rotate/shift/scale frame
 */
void warp(float *src, int w, int h, double dx, double dy, double dt, float filler)
{

    Mat inputImage(h, w, CV_32F, src);

    double s, c;

    sincos(dt, &s, &c);

    int flags = INTER_LANCZOS4;
    double m[2][3] = {{c, -s, dx}, {s, c, dy}};
    Mat affine(2, 3, CV_64F, m);

    Mat outputImage(inputImage.size(), inputImage.type());
    warpAffine(inputImage, outputImage, affine, inputImage.size(), flags, BORDER_CONSTANT, filler);

//    float *out = outputImage.ptr<float>();
//    memcpy(src, out, w * h * sizeof(float));
    outputImage.copyTo(inputImage);
}

/*
 * use OpenCV to find fit params
 */
void pairs_fit_params(GSList *pairs, double *dxo, double *dyo, double *dso, double *dto) {
    if (! (dxo || dyo || dso || dto) ) return;

    std::vector <Point2f> a;
    std::vector <Point2f> b;

    GSList *sl = pairs;
    while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
        if ((TYPE_MASK_GSTAR(gs) & TYPE_MASK_FRSTAR) == 0) {
//            err_printf("pairs_cs_diff: first star in pair must be a frstar \n");
            continue;
        }
        struct gui_star *gsp = GUI_STAR(gs->pair);
        if ((TYPE_MASK_GSTAR(gsp) & (TYPE_MASK_CATREF | TYPE_MASK_ALIGN)) == 0) {
 //           err_printf("pairs_cs_diff: second star in pair must be a catref \n");
            continue;
        }

        a.push_back(Point2f(gs->x, gs->y));
        b.push_back(Point2f(gsp->x, gsp->y));

        sl = g_slist_next(sl);
    }

    Mat result = estimateRigidTransform(a, b, false);

    if (dxo) *dxo = - result.at<double>(0, 2);
    if (dyo) *dyo = - result.at<double>(1, 2);
    double s = sqrt(sqr(result.at<double>(0, 0)) + sqr(result.at<double>(0, 1)));
    if (dso) *dso = sqrt(sqr(result.at<double>(0, 0)) + sqr(result.at<double>(0, 1)));
    if (dto) *dto = 180 / M_PI * atan2(result.at<double>(0, 1), result.at<double>(1, 1));
}

}
