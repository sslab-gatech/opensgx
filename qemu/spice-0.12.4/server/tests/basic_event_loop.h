#ifndef __BASIC_EVENT_LOOP_H__
#define __BASIC_EVENT_LOOP_H__

#include <spice.h>

SpiceCoreInterface *basic_event_loop_init(void);
void basic_event_loop_mainloop(void);

#endif // __BASIC_EVENT_LOOP_H__
