/*
 *      linkedlist.c
 *
 * This file contains the two structs used in the linkedList and a couple of functions to be implemented for
 * the linkedList.
 *
 */
#include "../config.h"

#ifndef linkedList_h
#define linkedList_h


struct node{
	int s;
	int identified;
	char name[MAX_NAME_SIZE + 1]; //one extra for \0
	struct node * next;
};

typedef struct{
	int count;
	struct node * head;
	struct node * tail;
} linkedList;

char* getNameBySocket(linkedList*, int);
int isIdentified(linkedList*, int);
void setNameBySocket(linkedList*, int, char*);
void initialize(linkedList*);
int isEmpty(linkedList*);
void push(linkedList*, int);;
void pop(linkedList*, int);

#endif
