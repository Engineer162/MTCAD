#include "mtcad/kernel.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "mt_thread.h"

mtcad_kernel_version mtcad_kernel_get_version(void) {
    mtcad_kernel_version version;
    version.major = 1;
    version.minor = 0;
    version.patch = 0;
    return version;
}

double mtcad_kernel_rectangle_area(double width, double height) {
    if (width < 0.0 || height < 0.0) {
        return -1.0;
    }
    return width * height;
}

static const double kPi = 3.14159265358979323846;

static double NormalizeOperationScale(int operation) {
    switch (operation) {
        case 1: return -1.0;  /* Cut */
        case 2: return 0.25;   /* Intersect */
        case 3: return 1.0;    /* New body */
        case 0:
        default:
            return 1.0;        /* Join */
    }
}

static mtcad_kernel_extrude_body_result EvaluateBody(const mtcad_kernel_extrude_body_input* input) {
    mtcad_kernel_extrude_body_result result;
    memset(&result, 0, sizeof(result));
    result.body_id = input->body_id;
    result.status = 0;

    const double safe_area = (input->profile_area >= 0.0) ? input->profile_area : 0.0;
    const double safe_depth = (input->depth >= 0.0) ? input->depth : 0.0;
    const double taper_radians = fabs(input->taper_angle_degrees) * (kPi / 180.0);
    const double taper_scale = 1.0 + (taper_radians * 0.25);
    const double body_scale = 1.0 + (0.08 * (double)((input->body_id >= 0 ? input->body_id : 0) % 7));
    const double operation_scale = NormalizeOperationScale(input->operation);

    const double effective_depth = safe_depth * taper_scale;
    const double base_volume = safe_area * effective_depth * body_scale;

    double integration_cost = 0.0;
    for (int step = 0; step < 160; ++step) {
        const double t = ((double)step + 0.5) / 160.0;
        const double wave = sin((t + body_scale) * taper_radians + safe_depth * 0.07);
        const double weight = 1.0 + (t * 0.5);
        integration_cost += fabs(wave) * weight;
    }

    result.effective_depth = effective_depth;
    result.estimated_volume_delta = base_volume * operation_scale;
    result.estimated_surface_work = integration_cost * safe_area;
    return result;
}

typedef struct worker_param {
    const mtcad_kernel_extrude_body_input* inputs;
    mtcad_kernel_extrude_body_result* outputs;
    size_t begin;
    size_t end;
} worker_param;

static void worker_thread_func(void* vparam) {
    worker_param* p = (worker_param*)vparam;
    const mtcad_kernel_extrude_body_input* inputs = p->inputs;
    mtcad_kernel_extrude_body_result* outputs = p->outputs;
    for (size_t i = p->begin; i < p->end; ++i) {
        outputs[i] = EvaluateBody(&inputs[i]);
    }
    free(p);
}

size_t mtcad_kernel_extrude_cut_parallel(
    const mtcad_kernel_extrude_body_input* inputs,
    size_t input_count,
    mtcad_kernel_extrude_body_result* outputs,
    size_t output_capacity,
    unsigned worker_count)
{
    if (inputs == NULL || outputs == NULL || input_count == 0 || output_capacity == 0) {
        return 0;
    }

    const size_t count = (input_count < output_capacity) ? input_count : output_capacity;
    unsigned hw_threads = mt_hardware_concurrency();
    size_t workers = (worker_count == 0) ? (hw_threads == 0 ? 4u : (size_t)hw_threads) : (size_t)worker_count;
    if (workers == 0) workers = 1;
    if (workers > count) workers = count;

    mt_thread* threads = (mt_thread*)malloc(sizeof(mt_thread) * workers);
    if (!threads) return 0;
    size_t base_chunk = count / workers;
    size_t remainder = count % workers;
    size_t begin = 0;

    for (size_t worker_index = 0; worker_index < workers; ++worker_index) {
        const size_t extra = worker_index < remainder ? 1u : 0u;
        const size_t chunk_size = base_chunk + extra;
        const size_t chunk_begin = begin;
        const size_t chunk_end = chunk_begin + chunk_size;
        begin = chunk_end;

        worker_param* p = (worker_param*)malloc(sizeof(worker_param));
        if (!p) {
            // join already created threads
            for (size_t j = 0; j < worker_index; ++j) {
                mt_thread_join(&threads[j]);
            }
            free(threads);
            return 0;
        }
        p->inputs = inputs;
        p->outputs = outputs;
        p->begin = chunk_begin;
        p->end = chunk_end;

        if (mt_thread_create(&threads[worker_index], worker_thread_func, p) != 0) {
            free(p);
            for (size_t j = 0; j < worker_index; ++j) {
                mt_thread_join(&threads[j]);
            }
            free(threads);
            return 0;
        }
    }

    for (size_t i = 0; i < workers; ++i) {
        mt_thread_join(&threads[i]);
    }

    free(threads);
    return count;
}

