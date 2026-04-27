#ifndef SIMPLELOGGING_H
#define SIMPLELOGGING_H
#include <stdint.h>

enum SL_LEVEL
{
    SL_FATAL   = 2, // "F"
    SL_ERROR   = 3, // "E"
    SL_WARNING = 4, // "W"
    SL_NOTICE  = 5, // "N"
    SL_DEBUG   = 7, // "D"
	SL_MAKE_ERROR_STRING = 99	//only for make error string
};
typedef void(* sl_onloghandler)(SL_LEVEL level, const char* name, const char* msg, void* user);

void        sl_setloghandler(sl_onloghandler handler, void* user);
void        sl_setloglevel(SL_LEVEL level);
SL_LEVEL	sl_getloglevel();
const char* sl_level2string(SL_LEVEL level);
void        sl_log(SL_LEVEL level, const char* name, const char *format, ...);

#endif // SIMPLELOGGING_H
