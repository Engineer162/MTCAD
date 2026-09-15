#ifndef MTCAD_KERNEL_H
#define MTCAD_KERNEL_H

#include "mtkernel/version.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
    #if defined(MTCAD_KERNEL_BUILD_DLL)
        #define MTCAD_KERNEL_API __declspec(dllexport)
    #elif defined(MTCAD_KERNEL_USE_DLL)
        #define MTCAD_KERNEL_API __declspec(dllimport)
    #else
        #define MTCAD_KERNEL_API
    #endif
#else
    #define MTCAD_KERNEL_API
#endif

typedef struct mtkernel_extrude_body_input {
    int body_id;
    double profile_area;
    double depth;
    double taper_angle_degrees;
    int operation;
} mtkernel_extrude_body_input;

typedef struct mtkernel_extrude_body_result {
    int body_id;
    double estimated_volume_delta;
    double estimated_surface_work;
    double effective_depth;
    int status;
} mtkernel_extrude_body_result;

MTCAD_KERNEL_API mtkernel_version mtkernel_get_version(void);
MTCAD_KERNEL_API double mtkernel_rectangle_area(double width, double height);
MTCAD_KERNEL_API size_t mtkernel_extrude_cut_parallel(
    const mtkernel_extrude_body_input* inputs,
    size_t input_count,
    mtkernel_extrude_body_result* outputs,
    size_t output_capacity,
    unsigned worker_count);

#ifdef __cplusplus
}
#endif

#endif
