#ifndef MTCAD_MT_THREAD_H
#define MTCAD_MT_THREAD_H

#include <stddef.h>
#include <uv.h>

typedef struct mt_thread {
    uv_thread_t handle;
} mt_thread;

typedef void (*mt_thread_func)(void* arg);

int mt_thread_create(mt_thread* thread, mt_thread_func func, void* arg);
int mt_thread_join(mt_thread* thread);
unsigned mt_hardware_concurrency(void);

#endif
