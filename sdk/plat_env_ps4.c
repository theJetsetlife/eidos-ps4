#include "plat_env_ps4.h"
#include <stddef.h>

#ifdef EIDOS_PLATFORM_PS4
char *bw_ps4_getenv(const char *name)
{
    (void)name;
    return NULL;
}
#endif
