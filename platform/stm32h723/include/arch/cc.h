#ifndef ITM_LWIP_ARCH_CC_H
#define ITM_LWIP_ARCH_CC_H

#include <stdint.h>

#define LWIP_PROVIDE_ERRNO
#define LWIP_TIMEVAL_PRIVATE 1
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

uint32_t itm_lwip_random(void);
#define LWIP_RAND() itm_lwip_random()
#define LWIP_PLATFORM_ASSERT(x) do { (void)(x); for (;;) {} } while (0)

#endif
