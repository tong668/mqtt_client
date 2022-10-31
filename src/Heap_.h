//
// Created by Administrator on 2022/10/31.
//

#ifndef ECLIPSE_PAHO_C_HEAP__H
#define ECLIPSE_PAHO_C_HEAP__H

///**
// * redefines malloc to use "mymalloc" so that heap allocation can be tracked
// * @param x the size of the item to be allocated
// * @return the pointer to the item allocated, or NULL
// */
//#define malloc(x) mymalloc(__FILE__, __LINE__, x)
//
///**
// * redefines realloc to use "myrealloc" so that heap allocation can be tracked
// * @param a the heap item to be reallocated
// * @param b the new size of the item
// * @return the new pointer to the heap item
// */
//#define realloc(a, b) myrealloc(__FILE__, __LINE__, a, b)
//
///**
// * redefines free to use "myfree" so that heap allocation can be tracked
// * @param x the size of the item to be freed
// */
//#define free(x) myfree(__FILE__, __LINE__, x)

#include <stddef.h>
/**
 * Information about the state of the heap.
 */
typedef struct
{
    size_t current_size;	/**< current size of the heap in bytes */
    size_t max_size;		/**< max size the heap has reached in bytes */
} heap_info;


void* mymalloc_(char*, int, size_t size);
void* myrealloc_(char*, int, void* p, size_t size);
void myfree_(char*, int, void* p);


int Heap_initialize_(void);
void Heap_terminate_(void);
heap_info* Heap_get_info_(void);
int HeapDump_(FILE* file);
int HeapDumpString_(FILE* file, char* str);
void* Heap_findItem_(void* p);
void Heap_unlink_(char* file, int line, void* p);


#endif //ECLIPSE_PAHO_C_HEAP__H
