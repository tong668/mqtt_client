//
// Created by Administrator on 2022/10/31.
//

#include <string.h>
#include <malloc.h>
#include "Heap_.h"
#include "Thread_.h"
#include "Tree_.h"
#include "Log_.h"


static pthread_mutex_t heap_mutex_store = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t *heap_mutex = &heap_mutex_store;

static heap_info state = {0, 0}; /**< global heap state information */

typedef double eyecatcherType;
static eyecatcherType eyecatcher = (eyecatcherType) 0x8888888888888888;

/**
 * Each item on the heap is recorded with this structure.
 */
typedef struct {
    char *file;        /**< the name of the source file where the storage was allocated */
    int line;        /**< the line no in the source file where it was allocated */
    void *ptr;        /**< pointer to the allocated storage */
    size_t size;    /**< size of the allocated storage */
#if defined(HEAP_STACK)
    char* stack;
#endif
} storageElement;


static Tree heap;    /**< Tree that holds the allocation records */


/**
 * Round allocation size up to a multiple of the size of an int.  Apart from possibly reducing fragmentation,
 * on the old v3 gcc compilers I was hitting some weird behaviour, which might have been errors in
 * sizeof() used on structures and related to packing.  In any case, this fixes that too.
 * @param size the size actually needed
 * @return the rounded up size
 */
static size_t Heap_roundup(size_t size) {
    static int multsize = 4 * sizeof(int);

    if (size % multsize != 0)
        size += multsize - (size % multsize);
    return size;
}


/**
 * Allocates a block of memory.  A direct replacement for malloc, but keeps track of items
 * allocated in a list, so that free can check that a item is being freed correctly and that
 * we can check that all memory is freed at shutdown.
 * @param file use the __FILE__ macro to indicate which file this item was allocated in
 * @param line use the __LINE__ macro to indicate which line this item was allocated at
 * @param size the size of the item to be allocated
 * @return pointer to the allocated item, or NULL if there was an error
 */
void *mymalloc_(char *file, int line, size_t size) {
    storageElement *s = NULL;
    size_t space = sizeof(storageElement);
    size_t filenamelen = strlen(file) + 1;
    void *rc = NULL;

    Thread_lock_mutex_(heap_mutex);
    size = Heap_roundup(size);
    if ((s = malloc(sizeof(storageElement))) == NULL) {
//        Log(LOG_ERROR, 13, errmsg);
        goto exit;
    }
    memset(s, 0, sizeof(storageElement));

    s->size = size; /* size without eyecatchers */
    if ((s->file = malloc(filenamelen)) == NULL) {
//        Log(LOG_ERROR, 13, errmsg);
        free(s);
        goto exit;
    }
    memset(s->file, 0, sizeof(filenamelen));

    space += filenamelen;
    strcpy(s->file, file);
#if defined(HEAP_STACK)
#define STACK_LEN 300
    if ((s->stack = malloc(STACK_LEN)) == NULL)
    {
        Log(LOG_ERROR, 13, errmsg);
        free(s->file);
        free(s);
        goto exit;
    }
    memset(s->stack, 0, sizeof(filenamelen));
    StackTrace_get(Thread_getid(), s->stack, STACK_LEN);
#endif
    s->line = line;
    /* Add space for eyecatcher at each end */
    if ((s->ptr = malloc(size + 2 * sizeof(eyecatcherType))) == NULL) {
//        Log(LOG_ERROR, 13, errmsg);
        free(s->file);
        free(s);
        goto exit;
    }
    memset(s->ptr, 0, size + 2 * sizeof(eyecatcherType));
    space += size + 2 * sizeof(eyecatcherType);
    *(eyecatcherType *) (s->ptr) = eyecatcher; /* start eyecatcher */
    *(eyecatcherType *) (((char *) (s->ptr)) + (sizeof(eyecatcherType) + size)) = eyecatcher; /* end eyecatcher */
//    Log(TRACE_MAX, -1, "Allocating %d bytes in heap at file %s line %d ptr %p\n", (int)size, file, line, s->ptr);
    TreeAdd_(&heap, s, space);
    state.current_size += size;
    if (state.current_size > state.max_size)
        state.max_size = state.current_size;
    rc = ((eyecatcherType *) (s->ptr)) + 1;    /* skip start eyecatcher */
    exit:
    Thread_unlock_mutex_(heap_mutex);
    return rc;
}

static void checkEyecatchers(char *file, int line, void *p, size_t size) {
    eyecatcherType *sp = (eyecatcherType *) p;
    char *cp = (char *) p;
    eyecatcherType us;
    static const char *msg = "Invalid %s eyecatcher %d in heap item at file %s line %d";

    if ((us = *--sp) != eyecatcher)
//        Log(LOG_ERROR, 13, msg, "start", us, file, line);
        printf("todo\n");

    cp += size;
    if ((us = *(eyecatcherType *) cp) != eyecatcher)
//        Log(LOG_ERROR, 13, msg, "end", us, file, line);
        printf("todo\n");
}


/**
 * Remove an item from the recorded heap without actually freeing it.
 * Use sparingly!
 * @param file use the __FILE__ macro to indicate which file this item was allocated in
 * @param line use the __LINE__ macro to indicate which line this item was allocated at
 * @param p pointer to the item to be removed
 */
static int Internal_heap_unlink(char *file, int line, void *p) {
    Node *e = NULL;
    int rc = 0;

    e = TreeFind_(&heap, ((eyecatcherType *) p) - 1);
    if (e == NULL)
//        Log(LOG_ERROR, 13, "Failed to remove heap item at file %s line %d", file, line);
        printf("todo\n");
    else {
        storageElement *s = (storageElement *) (e->content);
//        Log(TRACE_MAX, -1, "Freeing %d bytes in heap at file %s line %d, heap use now %d bytes\n",
//            (int) s->size, file, line, (int) state.current_size);
        checkEyecatchers(file, line, p, s->size);
        /* free(s->ptr); */
        free(s->file);
        state.current_size -= s->size;
        TreeRemoveNodeIndex_(&heap, e, 0);
        free(s);
        rc = 1;
    }
    return rc;
}


/**
 * Reallocates a block of memory.  A direct replacement for realloc, but keeps track of items
 * allocated in a list, so that free can check that a item is being freed correctly and that
 * we can check that all memory is freed at shutdown.
 * We have to remove the item from the tree, as the memory is in order and so it needs to
 * be reinserted in the correct place.
 * @param file use the __FILE__ macro to indicate which file this item was reallocated in
 * @param line use the __LINE__ macro to indicate which line this item was reallocated at
 * @param p pointer to the item to be reallocated
 * @param size the new size of the item
 * @return pointer to the allocated item, or NULL if there was an error
 */
void *myrealloc_(char *file, int line, void *p, size_t size) {
    void *rc = NULL;
    storageElement *s = NULL;

    Thread_lock_mutex_(heap_mutex);
    s = TreeRemoveKey_(&heap, ((eyecatcherType *) p) - 1);
    if (s == NULL)
        printf("todo\n");//todo
//        Log(LOG_ERROR, 13, "Failed to reallocate heap item at file %s line %d", file, line);
    else {
        size_t space = sizeof(storageElement);
        size_t filenamelen = strlen(file) + 1;

        checkEyecatchers(file, line, p, s->size);
        size = Heap_roundup(size);
        state.current_size += size - s->size;
        if (state.current_size > state.max_size)
            state.max_size = state.current_size;
        if ((s->ptr = realloc(s->ptr, size + 2 * sizeof(eyecatcherType))) == NULL) {
//            Log(LOG_ERROR, 13, errmsg);
            goto exit;
        }
        space += size + 2 * sizeof(eyecatcherType) - s->size;
        *(eyecatcherType *) (s->ptr) = eyecatcher; /* start eyecatcher */
        *(eyecatcherType *) (((char *) (s->ptr)) + (sizeof(eyecatcherType) + size)) = eyecatcher; /* end eyecatcher */
        s->size = size;
        space -= strlen(s->file);
        s->file = realloc(s->file, filenamelen);
        space += filenamelen;
        strcpy(s->file, file);
        s->line = line;
        rc = s->ptr;
        TreeAdd_(&heap, s, space);
    }
    exit:
    Thread_unlock_mutex_(heap_mutex);
    return (rc == NULL) ? NULL : ((eyecatcherType *) (rc)) + 1;    /* skip start eyecatcher */
}

/**
 * Frees a block of memory.  A direct replacement for free, but checks that a item is in
 * the allocates list first.
 * @param file use the __FILE__ macro to indicate which file this item was allocated in
 * @param line use the __LINE__ macro to indicate which line this item was allocated at
 * @param p pointer to the item to be freed
 */
void myfree_(char *file, int line, void *p) {
    if (p) /* it is legal und usual to call free(NULL) */
    {
        Thread_lock_mutex_(heap_mutex);
        if (Internal_heap_unlink(file, line, p))
            free(((eyecatcherType *) p) - 1);
        Thread_unlock_mutex_(heap_mutex);
    } else {
//        Log(LOG_ERROR, -1, "Call of free(NULL) in %s,%d",file,line);
    }
}

/**
 * List callback function for comparing storage elements
 * @param a pointer to the current content in the tree (storageElement*)
 * @param b pointer to the memory to free
 * @return boolean indicating whether a and b are equal
 */
static int ptrCompare(void* a, void* b, int value)
{
    a = ((storageElement*)a)->ptr;
    if (value)
        b = ((storageElement*)b)->ptr;

    return (a > b) ? -1 : (a == b) ? 0 : 1;
}


/**
 * Heap initialization.
 */
int Heap_initialize_(void)
{
    TreeInitializeNoMalloc_(&heap, ptrCompare);
    heap.heap_tracking = 0; /* no recursive heap tracking! */
    return 0;
}

/**
 * Scans the heap and reports any items currently allocated.
 * To be used at shutdown if any heap items have not been freed.
 */
static void HeapScan(enum LOG_LEVELS log_level)
{
    Node* current = NULL;

    Thread_lock_mutex_(heap_mutex);
//    Log(log_level, -1, "Heap scan start, total %d bytes", (int)state.current_size);
    while ((current = TreeNextElement_(&heap, current)) != NULL)
    {
        storageElement* s = (storageElement*)(current->content);
//        Log(log_level, -1, "Heap element size %d, line %d, file %s, ptr %p", (int)s->size, s->line, s->file, s->ptr);
//        Log(log_level, -1, "  Content %.*s", (10 > current->size) ? (int)s->size : 10, (char*)(((eyecatcherType*)s->ptr) + 1));
#if defined(HEAP_STACK)
        Log(log_level, -1, "  Stack:\n%s", s->stack);
#endif
    }
//    Log(log_level, -1, "Heap scan end");
    Thread_unlock_mutex_(heap_mutex);
}

/**
 * Heap termination.
 */
void Heap_terminate_(void)
{
//    Log(TRACE_MIN, -1, "Maximum heap use was %d bytes", (int)state.max_size);
    if (state.current_size > 20) /* One log list is freed after this function is called */
    {
//        Log(LOG_ERROR, -1, "Some memory not freed at shutdown, possible memory leak");
        HeapScan(LOG_ERROR);
    }
}

/**
 * Access to heap state
 * @return pointer to the heap state structure
 */
heap_info* Heap_get_info_(void)
{
    return &state;
}


/**
 * Dump a string from the heap so that it can be displayed conveniently
 * @param file file handle to dump the heap contents to
 * @param str the string to dump, could be NULL
 */
int HeapDumpString_(FILE* file, char* str)
{
    int rc = 0;
    size_t len = str ? strlen(str) + 1 : 0; /* include the trailing null */

    if (fwrite(&(str), sizeof(char*), 1, file) != 1)
        rc = -1;
    else if (fwrite(&(len), sizeof(int), 1 ,file) != 1)
        rc = -1;
    else if (len > 0 && fwrite(str, len, 1, file) != 1)
        rc = -1;
    return rc;
}


/**
 * Dump the state of the heap
 * @param file file handle to dump the heap contents to
 */
int HeapDump_(FILE* file)
{
    int rc = 0;
    Node* current = NULL;

    while (rc == 0 && (current = TreeNextElement_(&heap, current)))
    {
        storageElement* s = (storageElement*)(current->content);

        if (fwrite(&(s->ptr), sizeof(s->ptr), 1, file) != 1)
            rc = -1;
        else if (fwrite(&(current->size), sizeof(current->size), 1, file) != 1)
            rc = -1;
        else if (fwrite(s->ptr, current->size, 1, file) != 1)
            rc = -1;
    }
    return rc;
}

/**
 * Utility to find an item in the heap.  Lets you know if the heap already contains
 * the memory location in question.
 * @param p pointer to a memory location
 * @return pointer to the storage element if found, or NULL
 */
void* Heap_findItem_(void* p)
{
    Node* e = NULL;

    Thread_lock_mutex_(heap_mutex);
    e = TreeFind_(&heap, ((eyecatcherType*)p)-1);
    Thread_unlock_mutex_(heap_mutex);
    return (e == NULL) ? NULL : e->content;
}

/**
 * Remove an item from the recorded heap without actually freeing it.
 * Use sparingly!
 * @param file use the __FILE__ macro to indicate which file this item was allocated in
 * @param line use the __LINE__ macro to indicate which line this item was allocated at
 * @param p pointer to the item to be removed
 */
void Heap_unlink_(char* file, int line, void* p)
{
    Thread_lock_mutex_(heap_mutex);
    Internal_heap_unlink(file, line, p);
    Thread_unlock_mutex_(heap_mutex);
}




