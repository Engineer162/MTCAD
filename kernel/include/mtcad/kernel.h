#ifndef MTCAD_KERNEL_H
#define MTCAD_KERNEL_H

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

typedef struct mtcad_kernel_version {
    int major;
    int minor;
    int patch;
} mtcad_kernel_version;

typedef struct mtcad_kernel_extrude_body_input {
    int body_id;
    double profile_area;
    double depth;
    double taper_angle_degrees;
    int operation;
} mtcad_kernel_extrude_body_input;

typedef struct mtcad_kernel_extrude_body_result {
    int body_id;
    double estimated_volume_delta;
    double estimated_surface_work;
    double effective_depth;
    int status;
} mtcad_kernel_extrude_body_result;

MTCAD_KERNEL_API mtcad_kernel_version mtcad_kernel_get_version(void);
MTCAD_KERNEL_API double mtcad_kernel_rectangle_area(double width, double height);
MTCAD_KERNEL_API size_t mtcad_kernel_extrude_cut_parallel(
    const mtcad_kernel_extrude_body_input* inputs,
    size_t input_count,
    mtcad_kernel_extrude_body_result* outputs,
    size_t output_capacity,
    unsigned worker_count);

#ifdef __cplusplus
}
#endif

#endif
