#if defined(_MSC_VER)
#include <BaseTsd.h>
#define ssize_t_v SSIZE_T
#else
#define ssize_t_v ssize_t
#endif