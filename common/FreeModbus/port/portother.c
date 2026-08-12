#include "port.h"

static ULONG saved_primask;
static UCHAR critical_nesting;

void vMBPortEnterCritical(void)
{
	ULONG primask;

	__asm volatile("mrs %0, primask" : "=r"(primask));
	__asm volatile("cpsid i" ::: "memory");
	if (critical_nesting == 0U)
		saved_primask = primask;
	critical_nesting++;
}

void vMBPortExitCritical(void)
{
	if (critical_nesting == 0U)
		return;

	critical_nesting--;
	if ((critical_nesting == 0U) && ((saved_primask & 1U) == 0U))
		__asm volatile("cpsie i" ::: "memory");
}
