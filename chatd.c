/*
 *      chatd.c
 *
 * This file contains the main functions for the chatd server
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include "lib/linkedlist.h"
#include "config.h"

// descriptions at bottom near implementation.
void sendPacket(fd_set * list, int fdmax, int listener, int socket, const char* data);
void logger(FILE* logfile, const char * packet, int logLevel);
void sendUserError(int socket, const char* data);
void killUser(fd_set * list, linkedList * clients, int fdmax, int listener, int socket, FILE* logfile, int logLevel);
void safeExit(int exitCode, FILE* logfile, int talkinHole);

/*
 *
 * name: main
 *
 * @param	argc	the number of arguments
 * @param	argv	the arguments string
 * @return	any error codes needed
 */
int main(int argc, char **argv){


	FILE* logfile = NULL;
	int logLevel = 0;
	int opt;
	while ((opt = getopt(argc, argv, "lvch")) != -1) {
        	switch (opt) {
			case 'h':
				printf("CS360 Chat Server by Chris Corley");
				printf("Usage:\n\t%s [-lcvh] \n\n", argv[0]);
				printf("Options:\n\t-l\tLog all connects and disconnects to chatd-cs360.log");
				printf("\n\t-c\tDisplay all connects and disconnets on the server console");
				printf("\n\t-v\tDisplay all chat dialong on server console (verbose, implies c)");
				printf("\n\n-h\tDisplays this help message");				
				safeExit(0, logfile, 0);
			case 'l':
				logfile = fopen(SERVER_LOG_NAME, "w");
				if(logfile == NULL){
					printf("!! Could not open log file!");
					safeExit(1, logfile, 0);
				}
				logLevel += 1;
				break;
			case 'v':
				logLevel += 6; // v implies c, so 4 + 2;
				break;
			case 'c':
				logLevel += 2;
				break;
			default: /* '?' */		
				fprintf(stderr, "Usage: %s [-lcvh] \n",argv[0]);
				safeExit(1, logfile, 0);
		}
	}

	/* build the select stuff */
	fd_set readfds, master;
	FD_ZERO(&readfds);
	FD_ZERO(&master);
	int fdmax;

	struct sockaddr_in sin;
	socklen_t len; //needed for accept
	int ear, new_s;

	/* build address data structure */
	bzero((char *)&sin,sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(SERVER_PORT);

	/* setup passive open */
	if((ear = socket(PF_INET, SOCK_STREAM, 0)) < 0){
		logger(logfile, "!! Cannot get free socket.", logLevel);
		safeExit(1, logfile, ear);
	}
	int yes = 1;
	setsockopt(ear, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if((bind(ear, (struct sockaddr *)&sin, sizeof(sin))) < 0){
		logger(logfile, "!! Cannot bind to socket!", logLevel);
		safeExit(1, logfile, ear);
	}

	char buf[MAX_LINE];
	linkedList clients;
	initialize(&clients);

	listen(ear, MAX_PENDING);

	FD_SET(0, &master);
	FD_SET(ear, &master);
	fdmax = ear;

	int i; // for a loop below
	int bytes;
	int messagelen;
	int newMsgLen;
	char newMessage[MAX_LINE];
	char userName[MAX_NAME_SIZE];

	/* wait for connection, then receive and print text */
	while(1){
		bzero(buf, sizeof(buf));
		bzero(userName, sizeof(userName));
		bzero(newMessage, sizeof(newMessage));
		readfds = master;

		// poll the whole set
		if(select(fdmax+1, &readfds, NULL, NULL, NULL) == -1){
			logger(logfile, "!! Something is busted with select()... ", logLevel);
		}

		for(i=0;i<=fdmax;i++){
			if(FD_ISSET(i, &readfds)){
				if(i==0){
					//see if someone is typing or if enter was just pressed.
					if(fgetc(stdin) == '\n'){
						continue;
					}
				
					// tell our users we're going to be disconnecting them.
					newMsgLen = strlen("Server going down!");
					strcpy(newMessage, "ERR");
					newMessage[3] = (char)newMsgLen;
					strcat(newMessage, "Server going down!");
					sendPacket(&master, fdmax, ear, i, newMessage);

					// pull the plug
					safeExit(0, logfile, ear);
				}
				else if(i==ear){
					// new connection
					new_s = accept(ear, (struct sockaddr *)&sin, &len);
					if(new_s < 0){
						safeExit(1, logfile, ear);
					}
					else if(clients.count >= MAX_PENDING){
						sendUserError(new_s, "Server is full! Come back later.");	
						close(new_s);
					}
					else{
						FD_SET(new_s, &master);
						if(new_s > fdmax){
							fdmax = new_s;
						}
						push(&clients, new_s);
					}
				}
				else{
					// data
					if((bytes = recv(i, buf, sizeof(buf), 0)) <= 0){
						// client error/close	
						killUser(&master, &clients, fdmax, ear, i, logfile, logLevel);
					}
					else if(bytes > 3){	
						messagelen = (int)buf[3];
						if(messagelen > MAX_PACKET_SIZE - 4){
							//Packet is too big...
							bzero(buf, sizeof(buf));
							sendUserError(i, "Invalid packet! Cya!");	
							killUser(&master, &clients, fdmax, ear, i, logfile, logLevel);
						}
						else if(strncmp(buf, "NEW", 3) == 0){
							if(messagelen <= 25 && isIdentified(&clients, i) != 1){
								strncpy(userName, &buf[4], messagelen);
								setNameBySocket(&clients, i, userName);
								sendPacket(&master, fdmax, ear, i, buf);
								logger(logfile, buf, logLevel);
							}
							else{
								sendUserError(i, "User name too long or have already identified.");
								killUser(&master, &clients, fdmax, ear, i, logfile, logLevel);
							}
						}
						else if(strncmp(buf, "BYE", 3) ==0){
							if(messagelen <= 25){ 
								strncpy(userName, &buf[4], messagelen);
								userName[messagelen] = '\0';
								if(strcmp(userName, getNameBySocket(&clients, i)) == 0){
									killUser(&master, &clients, fdmax, ear, i, logfile, logLevel);
								}
								else{
									sendUserError(i, "Trying to quit a different user!");
									killUser(&master, &clients, fdmax, ear, i, logfile, logLevel);
								}
							}
						}

						else if(strncmp(buf, "MSG", 3) ==0){
							if(isIdentified(&clients, i)){
								strcpy(userName, getNameBySocket(&clients, i));
								newMsgLen = strlen(userName) + messagelen + 2;
								if(newMsgLen > MAX_PACKET_SIZE - 4){
									// not like this ever happens since the interface only allows 120 characters.
									sendUserError(i, "Message too long.");
								}
								strcpy(newMessage, "MSG");
								newMessage[3] = (char)newMsgLen;
								strcat(newMessage, userName);
								strcat(newMessage, ": ");
								strcat(newMessage, &buf[4]);
				
								sendPacket(&master, fdmax, ear, i, newMessage);
								logger(logfile, newMessage, logLevel);
							}
							else{
								sendUserError(i, "Identify first and then we'll talk!");
								killUser(&master, &clients, fdmax, ear, i, logfile, logLevel);
							}
						}
						else if(strncmp(buf, "ERR", 3) == 0){
							//errorz
							sendUserError(i, "Don't care about your problems.");
							killUser(&master, &clients, fdmax, ear, i, logfile, logLevel);
						}
						else{
							bzero(buf, sizeof(buf));
							logger(logfile, "!! User is talking gibberish! Disconnecting...", logLevel);
							killUser(&master, &clients, fdmax, ear, i, logfile, logLevel);
						}
					}
				}
			}
		}
	}
	
	safeExit(0, logfile, ear);

	return 0;
}

/*
 *
 * name: sendPacket
 *
 * Sends a packet to everyone in the list given, except for the listener and socket.
 *
 * @param	list	the fd_set containing the sockets to be sent to.
 * @param	fdmax	the largest socket number in the set, for looping.
 * @param	listener	the lister socket, used so we dont send() to it in the loop.
 * @param	socket	the socket who is sending the packet to everyone else, so we dont send() to it in the loop.
 * @param	data	the packet to be sent
 */
void sendPacket(fd_set * list, int fdmax, int listener, int socket,const char* data){
	int packetSize = strlen(data);
	int i;
	if(packetSize <= MAX_PACKET_SIZE){
		for(i=0;i<=fdmax;i++){
			if(FD_ISSET(i, list) && i!=listener && i!=socket){
				send(i, data, packetSize, 0);
			}
		}
	}
}

/*
 *
 * name: logger
 *
 * Logs any and all output depending on the logLevel given.
 *
 * @param	logfile	the file to be written to
 * @param	packet	the packet to be disected and read
 * @param	logLevel	the level of logging we need to do
 */
void logger(FILE* logfile, const char * packet, int logLevel){
	int dataLen = (int)packet[3];
	if (dataLen > MAX_PACKET_SIZE - 4){
		return;
	}

	char newPacket[MAX_LINE];
	if(strncmp(packet, "NEW", 3) == 0){
		strcpy(newPacket, "++ ");
	}
	else if(strncmp(packet, "BYE", 3) == 0){
		strcpy(newPacket, "-- ");
	}
	else if(strncmp(packet, "MSG", 3) == 0 && logLevel >= 4){
		// should just return if its a MSG and the logLevel is not 4 or more.
		strcpy(newPacket, ":D ");
	}
	else{
		return;
	}

	// copy in the payload and add a newline to the end
	strncat(newPacket, &packet[4], dataLen);
	strcat(newPacket, "\n");

	if(logLevel > 1){
		printf(newPacket);
	}
	if(logLevel % 2 == 1 && logfile != NULL){
		fprintf(logfile, newPacket);
	}
}


/*
 *
 * name: sendUserError
 * 
 * Sends a user an error message from the string of data given.
 *
 * @param	socket	socket on which the user is to be sent the error
 * @param	data	the string which the packet will contain in the payload of the error
 */
void sendUserError(int socket, const char* data){
	int newMsgLen = strlen(data);
	char newMessage[MAX_LINE];
	strcpy(newMessage, "ERR");
	newMessage[3] = (char)newMsgLen;
	strncat(newMessage, data, newMsgLen);
	newMessage[newMsgLen+4] = '\0';
	
	send(socket, newMessage, newMsgLen+4, 0);
}

/*
 *
 * name: killUser
 *
 * This is a megafunction which logs, disconnects a user, and sends a BYE message to all other users.
 *
 * @param	list	the list of users to be sent BYEs to
 * @param	clients	the linkedlist of users for fetching the name of the victim user
 * @param	fdmax	the largest socket number in the set, for looping.
 * @param	listener	the listener socket, so we don't send() to it.
 * @param	socket	the victim socket to be disconnected
 * @param	logfile	the log file to be written to
 * @param	logLevel	the level of logging needed
 */
void killUser(fd_set * list, linkedList * clients, int fdmax, int listener, int socket, FILE* logfile, int logLevel){
	if(isIdentified(clients,socket)){
		char userName[MAX_NAME_SIZE];
		char newMessage[MAX_LINE];
		bzero(userName, sizeof(userName));
		bzero(newMessage, sizeof(newMessage));

		strcpy(userName, getNameBySocket(clients, socket));
		strcpy(newMessage, "BYE");
		newMessage[3] = (char)strlen(userName);
		strcat(newMessage, userName);
	
		sendPacket(list, fdmax, listener, socket, newMessage);
		logger(logfile, newMessage, logLevel);
	}
	close(socket);
	pop(clients, socket);
	FD_CLR(socket, list);
}


/*
 * name: safeExit
 *
 * Ensures that the logfile is closed, the socket is closed and the interface shuts down.
 *
 * @param	exitCode	code to be sent to exit() when the function finishes other duties
 * @param	logfile	the log file to be closed
 * @param	talkinHole	the socket to be closed, usually the listener.
 */
void safeExit(int exitCode, FILE* logfile, int talkinHole){
	if(logfile != NULL){
		fclose(logfile);
	}
	if(talkinHole > 0){
	// cleanup time
		shutdown(talkinHole, 2);
		close(talkinHole);
	}

	
	exit(exitCode);
}

