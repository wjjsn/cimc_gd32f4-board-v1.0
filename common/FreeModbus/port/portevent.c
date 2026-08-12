#include "mb.h"
#include "mbport.h"

static volatile eMBEventType queued_event;
static volatile BOOL event_pending;

BOOL xMBPortEventInit(void)
{
	ENTER_CRITICAL_SECTION();
	event_pending = FALSE;
	EXIT_CRITICAL_SECTION();
	return TRUE;
}

BOOL xMBPortEventPost(eMBEventType event)
{
	ENTER_CRITICAL_SECTION();
	queued_event = event;
	event_pending = TRUE;
	EXIT_CRITICAL_SECTION();
	return TRUE;
}

BOOL xMBPortEventGet(eMBEventType *event)
{
	BOOL result = FALSE;

	ENTER_CRITICAL_SECTION();
	if (event_pending) {
		*event = queued_event;
		event_pending = FALSE;
		result = TRUE;
	}
	EXIT_CRITICAL_SECTION();
	return result;
}
