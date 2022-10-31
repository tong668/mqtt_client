//
// Created by Administrator on 2022/10/30.
//

#ifndef ECLIPSE_PAHO_C_TREE__H
#define ECLIPSE_PAHO_C_TREE__H

#include <stddef.h>

/**
 * Structure to hold all data for one list element
 */
typedef struct NodeStruct
{
    struct NodeStruct *parent,   /**< pointer to parent tree node, in case we need it */
    *child[2]; /**< pointers to child tree nodes 0 = left, 1 = right */
    void* content;				 /**< pointer to element content */
    size_t size;					 /**< size of content */
    unsigned int red : 1;
} Node;


/**
 * Structure to hold all data for one tree
 */
typedef struct
{
    struct
    {
        Node *root;	/**< root node pointer */
        int (*compare)(void*, void*, int); /**< comparison function */
    } index[2];
    int indexes,  /**< no of indexes into tree */
    count;    /**< no of items */
    size_t size;  /**< heap storage used */
    unsigned int heap_tracking : 1; /**< switch on heap tracking for this tree? */
    unsigned int allow_duplicates : 1; /**< switch to allow duplicate entries */
} Tree;


Tree* TreeInitialize_(int(*compare)(void*, void*, int));
void TreeInitializeNoMalloc_(Tree* aTree, int(*compare)(void*, void*, int));
void TreeAddIndex_(Tree* aTree, int(*compare)(void*, void*, int));
void* TreeAdd_(Tree* aTree, void* content, size_t size);

Node* TreeFind_(Tree* aTree, void* key);
Node* TreeFindIndex_(Tree* aTree, void* key, int index);
Node* TreeNextElement_(Tree* aTree, Node* curnode);

void* TreeRemoveNodeIndex_(Tree* aTree, Node* aNode, int index);
void* TreeRemove_(Tree* aTree, void* content);
void* TreeRemoveKey_(Tree* aTree, void* key);
void* TreeRemoveKeyIndex_(Tree* aTree, void* key, int index);
void TreeFree_(Tree* aTree);


int TreeIntCompare_(void* a, void* b, int);
int TreePtrCompare_(void* a, void* b, int);
int TreeStringCompare_(void* a, void* b, int);

#endif //ECLIPSE_PAHO_C_TREE__H
