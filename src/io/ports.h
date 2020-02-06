#ifndef PORTS_INTERFACE_H

#ifdef AMD64 /* If the ARCH is AMD64 */
#include "arch/amd64/io/ports.h"
#else /* Otherwise assume AMD64 */
#include "arch/amd64/io/ports.h"
#endif

#endif