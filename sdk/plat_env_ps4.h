#ifndef BW_PORT_PLAT_ENV_PS4_H
#define BW_PORT_PLAT_ENV_PS4_H

#include <stdlib.h>

#ifdef EIDOS_PLATFORM_PS4
/* Orbis applications do not receive a desktop process environment. Keep
 * environment-controlled diagnostics and developer overrides disabled; the
 * actual ROM, configuration, and save providers are separate platform APIs.
 */
char *bw_ps4_getenv(const char *name);
#define getenv bw_ps4_getenv
#endif

#endif
