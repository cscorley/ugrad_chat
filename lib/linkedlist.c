/*
 *      linkedlist.c
 *
 * This is the linkedList implementation of a true FIFO linkedList.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "linkedlist.h"
#include "../config.h"

/*
 *
 * name: isEmpty
 *
 * Checks to see if the given linkedList has any nodes in it.
 *
 * @param	l	the linkedList to be checked
 * @return	0 if false, 1 if true
 */
int isEmpty(linkedList * l) {
	return (l->count == 0);
}

/*
 *
 * name: initialize
 *
 * Simply insures that the linkedList's count is set to 0 before beginning.
 *
 * @param	l	the linkedList to be initialized
 */
void initialize(linkedList * l){
	l->count = 0;
}

/*
 *
 * name: findNode
 *
 * Searches the linked list for the given socket number and returns the pointer to the node
 *
 * @param	l	the linkedList to be searched
 * @param	socket	the socket to be searched for
 * @return	NULL if not found, the node otherwise
 */
struct node * findNode(linkedList * l, int socket){
	if(!isEmpty(l)){
		struct node * iter;
		iter = l->head;
		while(iter != NULL){
			if(socket==iter->s){
				return iter;
			}
			iter = iter->next;
		}
	}
	return NULL;
}

/*
 *
 * name: getNameBySocket
 *
 * Searches the list for a given socket and looks up the name at that socket.
 *
 * @param	l	the linkedList to be searched
 * @param	socket	the socket to be searched for
 * @return	the name of the user at that socket if identified, otherwise NULL
 */
char* getNameBySocket(linkedList * l, int socket){
	struct node * user = findNode(l, socket);
	if(user != NULL && user->identified){
		return user->name;
	}
	return NULL;
}


/*
 * name: setNameBySocket
 *
 * Finds the given socket in the list and sets the name
 *
 * @param	l	the linkedList to be searched
 * @param	socket	the socket to be searched for
 */
void setNameBySocket(linkedList * l, int socket, char* name){
	struct node * user = findNode(l, socket);
	if(user != NULL){
		strcpy(user->name, name);
		user->identified = 1;
		user->name[MAX_NAME_SIZE] = '\0';
	}
}

/*
 * name: isIdentified
 *
 * Finds the given socket in the list and checks to see if the user has identified with a name yet.
 *
 * @param	l	the linkedList to be searched
 * @param	socket	the socket to be searched for
 * @return	0 if the user has not identified, 1 otherwise
 */
int isIdentified(linkedList * l, int socket){
	struct node * user = findNode(l, socket);
	if(user != NULL){
		return user->identified;
	}
	return -1;
}

/*
 *
 * name: setIdentified
 *
 * Searches the list for the given socket and sets the idenfitied variable to true
 *
 * @param	l	the linkedList to be searched
 * @param	socket	the socket to be searched for
 */
void setIdentified(linkedList * l, int socket){
	struct node * user = findNode(l, socket);
	if(user != NULL){
		user->identified = 1;
	}
}

/*
 *
 * name: push
 *
 * Pushes the given socket onto the given linkedList in an order of arrival (FIFO)
 *
 * @param	l	the linkedList to be pushed onto
 * @param	s	the socket to be pushed onto the linked list
 */
void push(linkedList * l, int s){
	struct node * newNode;
	newNode = (struct node *)malloc(sizeof(struct node));
	if(newNode == NULL){
		printf("out of memory");
		exit(1);
	}
	
	newNode->s = s;
	newNode->identified = 0;
	newNode->next = NULL;

	if(!isEmpty(l)){
		l->tail->next = newNode;
		l->tail = newNode;
		l->count++;
	}
	else{
		l->head = newNode;
		l->tail = l->head;
		l->count = 1;
	}
}

/*
 *
 * name: pop
 *
 * Searches the list for the given socket and removes that node if found.
 *
 * @param	l	the linkedList to be searched
 * @param	socket	the socket to be searched for
 */
void pop(linkedList * l, int socket){
	struct node * deleting = findNode(l, socket);
	if(deleting != NULL){
		free(deleting);
		l->count--;
	}
}
