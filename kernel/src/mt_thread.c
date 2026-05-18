#include "mt_thread.h"
#include <stdlib.h>

int mt_thread_create(mt_thread* thread, mt_thread_func func, void* arg) {
    if (!thread || !func) return -1;
    int rc = uv_thread_create(&thread->handle, func, arg);
    return rc == 0 ? 0 : -1;
}

int mt_thread_join(mt_thread* thread) {
    if (!thread) return -1;
    uv_thread_join(&thread->handle);
    return 0;
}

unsigned mt_hardware_concurrency(void) {
    uv_cpu_info_t* infos = NULL;
    int count = 0;
    if (uv_cpu_info(&infos, &count) == 0 && count > 0) {
        uv_free_cpu_info(infos, count);
        return (unsigned)count;
    }
    return 1u;
}

