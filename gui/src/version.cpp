#include "version.h"

mtcad_application_version mtcad_application_get_version()
{
    return {
        MTCAD_VERSION_MAJOR,
        MTCAD_VERSION_MINOR,
        MTCAD_VERSION_PATCH
    };
}
