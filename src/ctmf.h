#ifndef CTMF_H
#define CTMF_H

#ifdef __cplusplus
extern "C" {
#endif

void ctmf(
        const float* src, float* dst,
        int width, int height,
        int src_step_row, int dst_step_row,
        int r, unsigned long memsize
        );

#ifdef __cplusplus
}
#endif

#endif
