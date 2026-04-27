#include "simplelogging.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <mutex>

static void* g_user = nullptr;
static SL_LEVEL g_loglevel = SL_LEVEL::SL_NOTICE;
static std::recursive_mutex g_logmutex;
static char g_logbuffer[4096];

const char* sl_level2string(SL_LEVEL level)
{
    if(level == SL_LEVEL::SL_DEBUG)
    {
        return "D";
    }
    else if(level == SL_LEVEL::SL_NOTICE)
    {
        return "N";
    }
    else if(level == SL_LEVEL::SL_WARNING)
    {
        return "W";
    }
    else if(level == SL_LEVEL::SL_ERROR)
    {
        return "E";
    }
    else if(level == SL_LEVEL::SL_FATAL)
    {
        return "F";
    }
    return "U";
}

static void on_logdefault(SL_LEVEL level, const char* name, const char* msg, void* user)
{
    if(level > g_loglevel)
    {
        return;
    }
    if((name != nullptr) && (strlen(name) > 0))
    {
        printf("[%s][%s] %s", sl_level2string(level), name, msg);
    }
    else
    {
        printf("[%s]%s", sl_level2string(level), msg);
    }
}

static sl_onloghandler g_onloghandler = on_logdefault;

void sl_setloghandler(sl_onloghandler handler, void* user)
{
    std::lock_guard<std::recursive_mutex> locker(g_logmutex);
    g_onloghandler = handler;
    g_user = user;
}

void sl_setloglevel(SL_LEVEL level)
{
    std::lock_guard<std::recursive_mutex> locker(g_logmutex);
    g_loglevel = level;
}

SL_LEVEL sl_getloglevel()
{
	return g_loglevel;
}

void sl_log(SL_LEVEL level, const char* name, const char *format, ...)
{
	if(level > sl_getloglevel())
	{
		return;
	}

    std::lock_guard<std::recursive_mutex> locker(g_logmutex);
	va_list ap;
	va_start(ap, format);
    memset(g_logbuffer, 0, sizeof(g_logbuffer));
	vsnprintf(g_logbuffer, sizeof(g_logbuffer)-1, format, ap);
	va_end(ap);
    g_onloghandler(level, name, g_logbuffer, g_user);
}
