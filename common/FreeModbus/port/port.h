#ifndef FREEMODBUS_PORT_H
#define FREEMODBUS_PORT_H

#ifdef __cplusplus
#define PR_BEGIN_EXTERN_C extern "C" {
#define PR_END_EXTERN_C }
#else
#define PR_BEGIN_EXTERN_C
#define PR_END_EXTERN_C
#endif

#define INLINE inline
#define ENTER_CRITICAL_SECTION() vMBPortEnterCritical()
#define EXIT_CRITICAL_SECTION() vMBPortExitCritical()
#define assert(expr) ((void)0)

typedef char BOOL;
typedef unsigned char UCHAR;
typedef char CHAR;
typedef unsigned short USHORT;
typedef short SHORT;
typedef unsigned long ULONG;
typedef long LONG;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

void vMBPortEnterCritical(void);
void vMBPortExitCritical(void);

#endif
