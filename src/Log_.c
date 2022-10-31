//
// Created by Administrator on 2022/10/31.
//

#include <stddef.h>
#include <malloc.h>
#include "Log_.h"
#include <string.h>
#include <pthread.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/time.h>
#include "Thread_.h"
#include "Messages_.h"

#define GETTIMEOFDAY 1

#define MAX_FUNCTION_NAME_LENGTH 256

#if !defined(_WIN32) && !defined(_WIN64)
/**
 * _unlink mapping for linux
 */
#define _unlink unlink
#endif

#if !defined(min)
#define min(A,B) ( (A) < (B) ? (A):(B))
#endif

trace_settings_type trace_settings_ =
        {
#if defined(HIGH_PERFORMANCE)
                LOG_ERROR,
#else
                TRACE_MINIMUM,
#endif
                400,
                INVALID_LEVEL
        };


typedef struct
{
#if defined(GETTIMEOFDAY)
    struct timeval ts;
#else
    struct timeb ts;
#endif
    int sametime_count;
    int number;
    pthread_t thread_id;
    int depth;
    char name[MAX_FUNCTION_NAME_LENGTH + 1];
    int line;
    int has_rc;
    int rc;
    enum LOG_LEVELS level;
} traceEntry;

static int start_index = -1,
        next_index = 0;
static traceEntry* trace_queue = NULL;
static int trace_queue_size = 0;
static FILE* trace_destination = NULL;	/**< flag to indicate if trace is to be sent to a stream */
static char* trace_destination_name = NULL; /**< the name of the trace file */
static char* trace_destination_backup_name = NULL; /**< the name of the backup trace file */

static int lines_written = 0; /**< number of lines written to the current output file */
static int max_lines_per_file = 1000; /**< maximum number of lines to write to one trace file */
static enum LOG_LEVELS trace_output_level = INVALID_LEVEL;

static char msg_buf[512];

static Log_traceCallback* trace_callback = NULL;

static int sametime_count = 0;

static pthread_mutex_t log_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t* log_mutex = &log_mutex_store;

struct timeval now_ts, last_ts;


static void Log_output(enum LOG_LEVELS log_level, const char *msg)
{
    if (trace_destination)
    {
        fprintf(trace_destination, "%s\n", msg);

        if (trace_destination != stdout && ++lines_written >= max_lines_per_file)
        {

            fclose(trace_destination);
            _unlink(trace_destination_backup_name); /* remove any old backup trace file */
            rename(trace_destination_name, trace_destination_backup_name); /* rename recently closed to backup */
            trace_destination = fopen(trace_destination_name, "w"); /* open new trace file */
            if (trace_destination == NULL)
                trace_destination = stdout;
            lines_written = 0;
        }
        else
            fflush(trace_destination);
    }

    if (trace_callback)
        (*trace_callback)(log_level, msg);
}

int Log_initialize_(Log_nameValue* info)
{
    int rc = -1;
    char* envval = NULL;
#if !defined(_WIN32) && !defined(_WIN64)
    struct stat buf;
#endif

    if ((trace_queue = malloc(sizeof(traceEntry) * trace_settings_.max_trace_entries)) == NULL)
        goto exit;
    trace_queue_size = trace_settings_.max_trace_entries;

    if ((envval = getenv("MQTT_C_CLIENT_TRACE")) != NULL && strlen(envval) > 0)
    {
        if (strcmp(envval, "ON") == 0 || (trace_destination = fopen(envval, "w")) == NULL)
            trace_destination = stdout;
        else
        {
            size_t namelen = 0;

            if ((trace_destination_name = malloc(strlen(envval) + 1)) == NULL)
            {
                free(trace_queue);
                goto exit;
            }
            strcpy(trace_destination_name, envval);
            namelen = strlen(envval) + 3;
            if ((trace_destination_backup_name = malloc(namelen)) == NULL)
            {
                free(trace_queue);
                free(trace_destination_name);
                goto exit;
            }
            if (snprintf(trace_destination_backup_name, namelen, "%s.0", trace_destination_name) >= namelen)
                trace_destination_backup_name[namelen-1] = '\0';
        }
    }
    if ((envval = getenv("MQTT_C_CLIENT_TRACE_MAX_LINES")) != NULL && strlen(envval) > 0)
    {
        max_lines_per_file = atoi(envval);
        if (max_lines_per_file <= 0)
            max_lines_per_file = 1000;
    }
    if ((envval = getenv("MQTT_C_CLIENT_TRACE_LEVEL")) != NULL && strlen(envval) > 0)
    {
        if (strcmp(envval, "MAXIMUM") == 0 || strcmp(envval, "TRACE_MAXIMUM") == 0)
            trace_settings_.trace_level = TRACE_MAXIMUM;
        else if (strcmp(envval, "MEDIUM") == 0 || strcmp(envval, "TRACE_MEDIUM") == 0)
            trace_settings_.trace_level = TRACE_MEDIUM;
        else if (strcmp(envval, "MINIMUM") == 0 || strcmp(envval, "TRACE_MINIMUM") == 0)
            trace_settings_.trace_level = TRACE_MINIMUM;
        else if (strcmp(envval, "PROTOCOL") == 0  || strcmp(envval, "TRACE_PROTOCOL") == 0)
            trace_output_level = TRACE_PROTOCOL;
        else if (strcmp(envval, "ERROR") == 0  || strcmp(envval, "TRACE_ERROR") == 0)
            trace_output_level = LOG_ERROR;
    }
    Log_output(TRACE_MINIMUM, "=========================================================");
    Log_output(TRACE_MINIMUM, "                   Trace Output");
    if (info)
    {
        while (info->name)
        {
            snprintf(msg_buf, sizeof(msg_buf), "%s: %s", info->name, info->value);
            Log_output(TRACE_MINIMUM, msg_buf);
            info++;
        }
    }
#if !defined(_WIN32) && !defined(_WIN64)
    if (stat("/proc/version", &buf) != -1)
    {
        FILE* vfile;

        if ((vfile = fopen("/proc/version", "r")) != NULL)
        {
            int len;

            strcpy(msg_buf, "/proc/version: ");
            len = strlen(msg_buf);
            if (fgets(&msg_buf[len], sizeof(msg_buf) - len, vfile))
                Log_output(TRACE_MINIMUM, msg_buf);
            fclose(vfile);
        }
    }
#endif
    Log_output(TRACE_MINIMUM, "=========================================================");
    exit:
    return rc;
}

void Log_terminate_(void)
{
    free(trace_queue);
    trace_queue = NULL;
    trace_queue_size = 0;
    if (trace_destination)
    {
        if (trace_destination != stdout)
            fclose(trace_destination);
        trace_destination = NULL;
    }
    if (trace_destination_name) {
        free(trace_destination_name);
        trace_destination_name = NULL;
    }
    if (trace_destination_backup_name) {
        free(trace_destination_backup_name);
        trace_destination_backup_name = NULL;
    }
    start_index = -1;
    next_index = 0;
    trace_output_level = INVALID_LEVEL;
    sametime_count = 0;
}

static traceEntry* Log_pretrace(void)
{
    traceEntry *cur_entry = NULL;

    /* calling ftime/gettimeofday seems to be comparatively expensive, so we need to limit its use */
    if (++sametime_count % 20 == 0)
    {
#if defined(GETTIMEOFDAY)
        gettimeofday(&now_ts, NULL);
		if (now_ts.tv_sec != last_ts.tv_sec || now_ts.tv_usec != last_ts.tv_usec)
#else
        ftime(&now_ts);
        if (now_ts.time != last_ts.time || now_ts.millitm != last_ts.millitm)
#endif
        {
            sametime_count = 0;
            last_ts = now_ts;
        }
    }

    if (trace_queue_size != trace_settings_.max_trace_entries)
    {
        traceEntry* new_trace_queue = malloc(sizeof(traceEntry) * trace_settings_.max_trace_entries);

        if (new_trace_queue == NULL)
            goto exit;
        memcpy(new_trace_queue, trace_queue, min(trace_queue_size, trace_settings_.max_trace_entries) * sizeof(traceEntry));
        free(trace_queue);
        trace_queue = new_trace_queue;
        trace_queue_size = trace_settings_.max_trace_entries;

        if (start_index > trace_settings_.max_trace_entries + 1 ||
            next_index > trace_settings_.max_trace_entries + 1)
        {
            start_index = -1;
            next_index = 0;
        }
    }

    /* add to trace buffer */
    cur_entry = &trace_queue[next_index];
    if (next_index == start_index) /* means the buffer is full */
    {
        if (++start_index == trace_settings_.max_trace_entries)
            start_index = 0;
    } else if (start_index == -1)
        start_index = 0;
    if (++next_index == trace_settings_.max_trace_entries)
        next_index = 0;
    exit:
    return cur_entry;
}

static char* Log_formatTraceEntry(traceEntry* cur_entry)
{
    struct tm *timeinfo;
    int buf_pos = 31;

#if defined(GETTIMEOFDAY)
    timeinfo = localtime((time_t *)&cur_entry->ts.tv_sec);
#else
    timeinfo = localtime(&cur_entry->ts.time);
#endif
    strftime(&msg_buf[7], 80, "%Y%m%d %H%M%S ", timeinfo);
#if defined(GETTIMEOFDAY)
    snprintf(&msg_buf[22], sizeof(msg_buf)-22, ".%.3lu ", cur_entry->ts.tv_usec / 1000L);
#else
    snprintf(&msg_buf[22], sizeof(msg_buf)-22, ".%.3hu ", cur_entry->ts.millitm);
#endif
    buf_pos = 27;

    snprintf(msg_buf, sizeof(msg_buf), "(%.4d)", cur_entry->sametime_count);
    msg_buf[6] = ' ';

    if (cur_entry->has_rc == 2)
        strncpy(&msg_buf[buf_pos], cur_entry->name, sizeof(msg_buf)-buf_pos);
    else
    {
        const char *format = Messages_get_(cur_entry->number, cur_entry->level);//todo messges_get 稍后处理
        if (cur_entry->has_rc == 1)
            snprintf(&msg_buf[buf_pos], sizeof(msg_buf)-buf_pos, format, cur_entry->thread_id,
                     cur_entry->depth, "", cur_entry->depth, cur_entry->name, cur_entry->line, cur_entry->rc);
        else
            snprintf(&msg_buf[buf_pos], sizeof(msg_buf)-buf_pos, format, cur_entry->thread_id,
                     cur_entry->depth, "", cur_entry->depth, cur_entry->name, cur_entry->line);
    }
    return msg_buf;
}


static void Log_posttrace(enum LOG_LEVELS log_level, traceEntry* cur_entry)
{
    if (((trace_output_level == -1) ? log_level >= trace_settings_.trace_level : log_level >= trace_output_level))
    {
        char* msg = NULL;

        if (trace_destination || trace_callback)
            msg = &Log_formatTraceEntry(cur_entry)[7];

        Log_output(log_level, msg);
    }
}



static void Log_trace(enum LOG_LEVELS log_level, const char *buf)
{
    traceEntry *cur_entry = NULL;

    if (trace_queue == NULL)
        return;

    cur_entry = Log_pretrace();

    memcpy(&(cur_entry->ts), &now_ts, sizeof(now_ts));
    cur_entry->sametime_count = sametime_count;

    cur_entry->has_rc = 2;
    strncpy(cur_entry->name, buf, sizeof(cur_entry->name));
    cur_entry->name[MAX_FUNCTION_NAME_LENGTH] = '\0';

    Log_posttrace(log_level, cur_entry);
}

/**
 * Log a message.  If possible, all messages should be indexed by message number, and
 * the use of the format string should be minimized or negated altogether.  If format is
 * provided, the message number is only used as a message label.
 * @param log_level the log level of the message
 * @param msgno the id of the message to use if the format string is NULL
 * @param aFormat the printf format string to be used if the message id does not exist
 * @param ... the printf inserts
 */
void Log_(enum LOG_LEVELS log_level, int msgno, const char *format, ...)
{
    if (log_level >= trace_settings_.trace_level)
    {
        const char *temp = NULL;
        va_list args;

        /* we're using a static character buffer, so we need to make sure only one thread uses it at a time */
        Thread_lock_mutex_(log_mutex);
        if (format == NULL && (temp = Messages_get_(msgno, log_level)) != NULL)//todo messges_get 稍后处理
            format = temp;

        va_start(args, format);
        vsnprintf(msg_buf, sizeof(msg_buf), format, args);

        Log_trace(log_level, msg_buf);
        va_end(args);
        Thread_unlock_mutex_(log_mutex);
    }
}


/**
 * The reason for this function is to make trace logging as fast as possible so that the
 * function exit/entry history can be captured by default without unduly impacting
 * performance.  Therefore it must do as little as possible.
 * @param log_level the log level of the message
 * @param msgno the id of the message to use if the format string is NULL
 * @param aFormat the printf format string to be used if the message id does not exist
 * @param ... the printf inserts
 */
void Log_stackTrace_(enum LOG_LEVELS log_level, int msgno, pthread_t thread_id, int current_depth, const char* name, int line, int* rc)
{
    traceEntry *cur_entry = NULL;

    if (trace_queue == NULL)
        return;

    if (log_level < trace_settings_.trace_level)
        return;

    Thread_lock_mutex_(log_mutex);
    cur_entry = Log_pretrace();

    memcpy(&(cur_entry->ts), &now_ts, sizeof(now_ts));
    cur_entry->sametime_count = sametime_count;
    cur_entry->number = msgno;
    cur_entry->thread_id = thread_id;
    cur_entry->depth = current_depth;
    strcpy(cur_entry->name, name);
    cur_entry->level = log_level;
    cur_entry->line = line;
    if (rc == NULL)
        cur_entry->has_rc = 0;
    else
    {
        cur_entry->has_rc = 1;
        cur_entry->rc = *rc;
    }

    Log_posttrace(log_level, cur_entry);
    Thread_unlock_mutex_(log_mutex);
}

void Log_setTraceCallback_(Log_traceCallback* callback)
{
    trace_callback = callback;
}

void Log_setTraceLevel_(enum LOG_LEVELS level)
{
    if (level <= LOG_ERROR) /* the lowest we can go is LOG_ERROR */
        trace_settings_.trace_level = level;
    trace_output_level = level;
}

