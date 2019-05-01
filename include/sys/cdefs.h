#ifndef _ZOL_SYS_CDEFS_H_
#define _ZOL_SYS_CDEFS_H_

#if !defined(__FreeBSD__)
#if !__GNUC_PREREQ__(2, 7) && !defined(__INTEL_COMPILER)
#define __printflike(fmtarg, firstvararg)
#define __scanflike(fmtarg, firstvararg)
#define __format_arg(fmtarg)
#define __strfmonlike(fmtarg, firstvararg)
#define __strftimelike(fmtarg, firstvararg)
#else
#define __printflike(fmtarg, firstvararg) \
            __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#define __scanflike(fmtarg, firstvararg) \
            __attribute__((__format__ (__scanf__, fmtarg, firstvararg)))
#define __format_arg(fmtarg)    __attribute__((__format_arg__ (fmtarg)))
#define __strfmonlike(fmtarg, firstvararg) \
            __attribute__((__format__ (__strfmon__, fmtarg, firstvararg)))
#define __strftimelike(fmtarg, firstvararg) \
            __attribute__((__format__ (__strftime__, fmtarg, firstvararg)))
#endif
#endif
#include_next<sys/cdefs.h>

#endif
