//
// Created by Administrator on 2022/10/31.
//

#ifndef ECLIPSE_PAHO_C_LOG__H
#define ECLIPSE_PAHO_C_LOG__H

#include <pthread.h>

enum LOG_LEVELS {
    INVALID_LEVEL = -1,
    TRACE_MAXIMUM = 1,
    TRACE_MEDIUM,
    TRACE_MINIMUM,
    TRACE_PROTOCOL,
    LOG_ERROR,
    LOG_SEVERE,
    LOG_FATAL,
};

typedef struct
{
    enum LOG_LEVELS trace_level;	/**< trace level */
    int max_trace_entries;		/**< max no of entries in the trace buffer */
    enum LOG_LEVELS trace_output_level;		/**< trace level to output to destination */
} trace_settings_type;

 extern trace_settings_type trace_settings_;


typedef struct
{
    const char* name;
    const char* value;
} Log_nameValue;

typedef void Log_traceCallback(enum LOG_LEVELS level, const char *message);


int Log_initialize_(Log_nameValue*);
void Log_terminate_(void);

void Log_(enum LOG_LEVELS, int, const char *, ...);
void Log_stackTrace_(enum LOG_LEVELS, int, pthread_t, int, const char*, int, int*);

void Log_setTraceCallback_(Log_traceCallback* callback);
void Log_setTraceLevel_(enum LOG_LEVELS level);


#endif //ECLIPSE_PAHO_C_LOG__H
