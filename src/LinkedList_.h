//
// Created by Administrator on 2022/10/30.
//

#ifndef ECLIPSE_PAHO_C_LINKEDLIST__H
#define ECLIPSE_PAHO_C_LINKEDLIST__H

#include <stddef.h>

/**
 * Structure to hold all data for one list element
 */
typedef struct ListElementStruct
{
    struct ListElementStruct *prev, /**< pointer to previous list element */
    *next;	/**< pointer to next list element */
    void* content;					/**< pointer to element content */
} ListElement;


/**
 * Structure to hold all data for one list
 */
typedef struct
{
    ListElement *first,	/**< first element in the list */
    *last,	/**< last element in the list */
    *current;	/**< current element in the list, for iteration */
    int count;  /**< no of items */
    size_t size;  /**< heap storage used */
} List;

void ListZero_(List*);
List* ListInitialize_(void);

ListElement* ListAppend_(List* aList, void* content, size_t size);
void ListAppendNoMalloc_(List* aList, void* content, ListElement* newel, size_t size);
ListElement* ListInsert_(List* aList, void* content, size_t size, ListElement* index);

ListElement* ListNextElement_(List* aList, ListElement** pos);
ListElement* ListPrevElement_(List* aList, ListElement** pos);

ListElement* ListFind_(List* aList, void* content);
ListElement* ListFindItem_(List* aList, void* content, int(*callback)(void*, void*));

int ListRemove_(List* aList, void* content);
int ListRemoveItem_(List* aList, void* content, int(*callback)(void*, void*));
void* ListDetachHead_(List* aList);
int ListRemoveHead_(List* aList);
void* ListPopTail_(List* aList);

int ListDetach_(List* aList, void* content);
int ListDetachItem_(List* aList, void* content, int(*callback)(void*, void*));

void ListFree_(List* aList);
void ListEmpty_(List* aList);
void ListFreeNoContent_(List* aList);


int intcompare_(void* a, void* b);
int stringcompare_(void* a, void* b);






#endif //ECLIPSE_PAHO_C_LINKEDLIST__H
