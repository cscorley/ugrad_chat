/*
 *      chatc.c
 *
 * This file contains the main functions for the chat client
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <curses.h>
#include "lib/chat-display.h"
#include "config.h"

// descriptions near the bottom with the implemenations.
void logger(FILE* logfile, int logLevel, const char* data);
int sendMessage(int socket, const char* type, const char* data);
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
	// a few things for the options passed to the program
	char argUserName[MAX_NAME_SIZE];
	char * argServerAddress;
	FILE* logfile = NULL;
	int logLevel = 0; 
	int cFlag = 0;
	int nFlag = 0;
	int opt;

	// get the passed options from the user
	// options c and n are required to run.
	while ((opt = getopt(argc, argv, "lvhn:c:")) != -1) {
        	switch (opt) {
			case 'h':
				printf("CS360 Chat Client by Chris Corley");
				printf("Usage: %s -c server_address -n name [-lvh]\n\n", argv[0]);
				printf("Required:\n\t-c server_address \t Connect to server given.");
				printf("\n\t-n user_chat_name \t User name to be used in chat.\n\n");
				printf("Options:\n\t-l\tLog initialization messages and errors to chat-client.log");
				printf("\n\t-v\tVerbose, display all initialization and shutdown messages");
				printf("\n\n\t-h\tDisplays this help message\n");				
				safeExit(0, logfile, 0);		
			case 'c':
				cFlag = 1;
				argServerAddress = (char *)malloc(strlen(optarg));
				if(argServerAddress == NULL){
					printf("Can't get memory!");
					safeExit(1, logfile, 0);
				}
				strcpy(argServerAddress, optarg);
				break;
			case 'n':
				nFlag = 1;
				bzero(argUserName, sizeof(argUserName));
				if(strlen(optarg) > 25){
					printf("User name too long!");
					safeExit(1, logfile, 0);
				}
				strcpy(argUserName, optarg);
				argUserName[MAX_NAME_SIZE-1] = '\0';
				break;
			case 'l':
				logLevel += 1;
				logfile = fopen(CLIENT_LOG_NAME, "w");
				if(logfile == NULL){
					printf("Could not open log file!");
					safeExit(1, logfile, 0);
				}
				break;
			case 'v':
				logLevel += 2;
				break;
			default: /* '?' */
				fprintf(stderr, "Usage: %s -c server_address -n name [-lvh] \n",argv[0]);
				safeExit(1, logfile, 0);
		}
	}

	// did what they say meet the requirements?
	if(!cFlag && !nFlag){
		fprintf(stderr, "Usage: %s -c server_address -n name [-lvh] \n",argv[0]);
		safeExit(1, logfile, 0);
	}

	
	struct hostent *hp;
	// see if we can resolve this address
	hp = gethostbyname(argServerAddress);
	if(!hp){
		struct in_addr ipv4addr;
		inet_pton(AF_INET, argServerAddress, &ipv4addr);
		hp = gethostbyaddr(&ipv4addr, sizeof(ipv4addr), AF_INET);
		if(!hp){
			logger(logfile, logLevel, "Cannot find host.");	
			safeExit(1, logfile, 0);
		}
	}

	//build address data structure
	struct sockaddr_in sin;
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
	sin.sin_port = htons(SERVER_PORT);

	int talkinHole;
	// see if we can get us a talkinHole...	
	if ((talkinHole = socket(PF_INET, SOCK_STREAM, 0)) < 0){
		logger(logfile, logLevel, "Cannot get free socket.");
		safeExit(1, logfile, talkinHole);
	}

	// see if the server picks up on their side of the talkinHole
	if (connect(talkinHole, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		close(talkinHole);
		logger(logfile, logLevel, "Cannot connect to server.");
		safeExit(1, logfile, talkinHole);
	}

	// bring up the chat interface
	if(!initialize_display(argServerAddress, strlen(argServerAddress), argUserName, strlen(argUserName))){
		logger(logfile, logLevel, "Cannot intialize ncurses display!");
		safeExit(1, logfile, talkinHole);	
	}
	
	// a few buffers we'll use throughout the main loop.
	char buf[MAX_LINE];
	char newMessage[MAX_LINE];
	char errMessage[MAX_LINE];

	// huzzah! we're in... ready to rock and roll.
	// lets let 'em know:
	if(sendMessage(talkinHole, "NEW", argUserName) < 1){
		safeExit(1, logfile, talkinHole);
	}

	// going to need these for select() in the main loop
	fd_set reader;
	FD_ZERO(&reader);

	// a couple integers we'll use throughout the main loop.
	int bytes = 0;
	int messageLen = 0;
	
	// main loop: get and send lines of text
	while(1){			
		// shake off any excess junk from the last loop
		bzero(buf, sizeof(buf));

		// see if the listener(talkinHole) and stdin(0) sockets have anything.
		FD_SET(talkinHole, &reader);
		FD_SET(0, &reader);
		select(talkinHole+1, &reader, NULL, NULL, NULL);

		// check if stdin(0) is ready
		if(FD_ISSET(0, &reader)){
			// this next call is going to block until the user hits enter
			get_chat_message(buf, sizeof(buf));

			if(strcmp(buf, "EXIT")==0){
				sendMessage(talkinHole, "BYE", argUserName);
				break;
			}
			sendMessage(talkinHole, "MSG", buf);

			// attach a name and put that on the users interface
			strcpy(newMessage, argUserName);
			strcat(newMessage, ": ");
			strcat(newMessage, buf);
			put_chat_message(newMessage);
		}

		// clear out any user stuff just incase, probably not needed
		bzero(buf, sizeof(buf));

		// check to see if the talkinHole is makin' any noise
		if(FD_ISSET(talkinHole, &reader)){
			bytes = recv(talkinHole, buf, sizeof(buf), 0);
			if(bytes == 0){
				strcpy(errMessage, "SERVER ERROR: Connection lost.  Press any key to exit.");
				logger(logfile, logLevel, errMessage);
				fgetc(stdin);
				safeExit(1, logfile, talkinHole);
			}
			else if(bytes > 4){
				// make sure theres atleast one byte of payload. 
				// wasting our time otherwise.
				
				messageLen = (int)buf[3];
				if(messageLen > MAX_PACKET_SIZE - 4){
					// just going to ignore the packet being too big even happened.
					bzero(buf, sizeof(buf));
				}
				else if(strncmp(buf, "NEW", 3) == 0){
					strcpy(newMessage, &buf[4]);
					strcat(newMessage, " has joined.");
					put_chat_message(newMessage);
				}
				else if(strncmp(buf, "BYE", 3) ==0){
					strcpy(newMessage, &buf[4]);
					strcat(newMessage, " has left.");
					put_chat_message(newMessage);
				}
				else if(strncmp(buf, "MSG", 3) ==0){
					strcpy(newMessage, &buf[4]);
					newMessage[messageLen] = '\0';
					put_chat_message(newMessage);
				}
				else if(strncmp(buf, "ERR", 3) == 0){
					strcpy(errMessage, "SERVER ERROR: ");
					strcat(errMessage, &buf[4]);
					logger(logfile, logLevel, errMessage);
				}
				else{
					// if we can't atleast talk in the right protocol, perhaps we should just end this long distance relationship.
					bzero(buf, sizeof(buf));
					strcpy(errMessage, "SERVER ERROR: Server is talking gibberish!  Press any key to exit.");
					logger(logfile, logLevel, errMessage);
					fgetc(stdin);
					safeExit(0, logfile, talkinHole);
				}
			}
		}
	}

	safeExit(0, logfile, talkinHole);

	// should be no real reason for this.
	return 0;
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
void logger(FILE* logfile, int logLevel,const char* data){
	if(logLevel % 2 == 0 && logfile != NULL){
		fprintf(logfile, data);
	}
	if(logLevel > 1){
		fprintf(stderr, data);
	}
}

/*
 *
 * name: sendMessage
 *
 * Sends a message on the given socket after building a packet of the given type with the data payload
 *
 * @param	socket	the socket to send the message on
 * @param	type	the type of packet to be sent, eg "NEW", "BYE", "MSG", "ERR".
 * @param	data	the data to be within the payload of the packet.
 *
 */
int sendMessage(int socket, const char* type,const char* data){
	int payloadSize = strlen(data);
	if(payloadSize < MAX_PACKET_SIZE - 4){
		char newMessage[MAX_PACKET_SIZE];
		bzero(newMessage, sizeof(newMessage));
		strncpy(newMessage, type, 3);
		newMessage[3] = (char)payloadSize;
		strncat(newMessage, data, payloadSize);
		return send(socket, newMessage, payloadSize+4, 0);
	}
	return 0;
}

/*
 * name: safeExit
 *
 * Ensures that the logfile is closed, the socket is closed and the interface shuts down.
 *
 * @param	exitCode	code to be sent to exit() when the function finishes other duties
 * @param	logfile	the log file to be closed
 * @param	talkinHole	the socket to be closed
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

	shutdown_display();

	exit(exitCode);
}


