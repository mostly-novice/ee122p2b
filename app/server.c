// Server code
#include <stdio.h>
#include <stdlib.h>
#include "constants.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>

#include "header.h"
#include "messages.h"
#include "server_output.h"

#define STDIN 0
#define HEADER_LENGTH 4
#define DIR "users"

// Flags
#define HEADER 0
#define PAYLOAD 1

#define MAX_CONNECTION 30
#define MAX_MESSAGE_RECORD 50

#ifndef max
#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif

#ifndef min
#define min( a, b ) ( ((a) < (b)) ? (a) : (b) )
#endif

enum {
		FAULT_TYPE_NONE = 0,
		FAULT_LOSSY_LINK,
		FAULT_MESSAGE_TYPE,
		FAULT_ERROR_CODE,
};

static unsigned s_fault = FAULT_TYPE_NONE;

struct input {
		unsigned int ip;
		unsigned short port;
		unsigned char lastbyte;
}__attribute__((packed));

typedef struct r{
		unsigned int low;
		unsigned int high;
} Range;

typedef struct mr{
		unsigned int ip;
		unsigned int id;
		unsigned char * message;
} message_record;

typedef struct buffer{
		int flag;
		int desire_length;
		int buffer_size;
		char * buffer;
} bufferdata;

int findDup(message_record ** mr_array,int id, int ip){
		int i;
		for(i = 0; i <MAX_MESSAGE_RECORD;i++){
				message_record * mr = mr_array[i];
				if(mr){
						if(mr->ip == ip && mr->id == id){
								return i;
						}
				}
		}
		return -1;
}

void printMessage(char * message, int len){
		int i;
		for(i = 0; i < len; i++){
				printf("%02x ", *(message+i));
		}
		printf("\n");
}

void cleanBuffer(bufferdata ** fdbuffermap,int i){
		if(fdbuffermap[i]){
				if(fdbuffermap[i]->buffer)
						free(fdbuffermap[i]->buffer);
				free(fdbuffermap[i]);

				bufferdata * bufferd = (bufferdata *) malloc(sizeof(bufferdata));
				bufferd->flag = HEADER;
				bufferd->desire_length = HEADER_LENGTH;
				bufferd->buffer_size = 0;
				bufferd->buffer = NULL;

				fdbuffermap[i] = bufferd;
		}
}

void cleanNameMap(char ** fdnamemap,int i){
		printf("Cleaning up name map \n");
		if(fdnamemap[i]){
				free(fdnamemap[i]);
				fdnamemap[i] = NULL;
		}
}

#include "model.h"
#include "p2p.h"
#include "processHelper.h"
#include "aux.h"

int main(int argc, char* argv[]){

		/*
		   These are pointer to serverInstance for predecessor and successor
		   */

		serverInstance * pred_si;
		serverInstance * succ_si;

		int pred_sock = -1;
		int succ_sock = -1;

		Range * primary = (Range *) malloc (sizeof(Range));
		Range * backup  = (Range *) malloc (sizeof(Range));

		unsigned int p2p_id; // [0-1023]

		// Model Variables
		LinkedList * primarylist = (LinkedList *) malloc (sizeof(LinkedList));
		primarylist->head = NULL;
		primarylist->tail = NULL;

		LinkedList * backuplist = (LinkedList *) malloc (sizeof(LinkedList));
		backuplist->head = NULL;
		backuplist->tail = NULL;

		struct sockaddr_in client_sin;
		Node * p;
		char command[80];
		char arg[4000];
		struct timeval tv;
		int oldest = 0;

		srand(time(NULL));
		int id = rand();

		int listener;
		int udplistener;
		int myport;
		int myudpport;
		int done = 0;
		int status;

		message_record ** mr_array = malloc(sizeof(*mr_array)*MAX_MESSAGE_RECORD);

		fd_set master;
		fd_set readfds;
		fd_set login;
		int fdmax;

		char ** fdnamemap = malloc(sizeof(*fdnamemap)*MAX_CONNECTION);
		bufferdata ** fdbuffermap = malloc(sizeof(*fdbuffermap)*MAX_CONNECTION);
		int k;
		for(k=0; k<MAX_CONNECTION; ++k ){
				fdnamemap[k] = NULL;
				bufferdata * bufferd = (bufferdata *) malloc(sizeof(bufferdata));
				bufferd->flag = HEADER;         // error: parse error before '.' token
				bufferd->desire_length = HEADER_LENGTH;
				bufferd->buffer_size = 0;
				bufferd->buffer = NULL;
				fdbuffermap[k] = bufferd;
		}

		struct sockaddr_in pred_sin;
		memset(&pred_sin, 0, sizeof(pred_sin));

		struct sockaddr_in succ_sin;
		memset(&succ_sin, 0, sizeof(succ_sin));

		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));

		struct sockaddr_in udpsin;
		memset(&udpsin, 0, sizeof(udpsin));
		int sin_len;

		// Error  flags
		int badMessageTypeFlag = 0;
		int lossy = 0;
		int faultyErrorCodeFlag = 0;
		int lossycount = 0;
		// Initilizations
		int c;
		char* uvalue=NULL;
		char* tvalue=NULL;

		while( (c=getopt( argc,argv,"u:t:f:"))!=-1){
				switch(c){
						case 'u': uvalue=optarg; break;
						case 't': tvalue=optarg; break;
						default:
								  printf("Usage: ./server -t <server TCP port> -u <server UDP port");
								  return 0;
				}
		}


		myport = atoi(tvalue);
		myudpport = atoi(uvalue);
		if(setvbuf(stdout,NULL,_IONBF,NULL) != 0){
				perror('setvbuf');
		}

		// Find the nonlocal_ip
		char hostname[30];
		gethostname(hostname,30);
		struct hostent * hp = gethostbyname(hostname);

		struct sockaddr_in mysin;

		char * ipchar = inet_ntoa(*(struct in_addr*)(hp->h_addr_list[0]));
		inet_aton(ipchar,&mysin.sin_addr);
		int nonlocalip = mysin.sin_addr.s_addr;

		printf("Hostname:%s\n",hostname);
		printf("Local Ip:%s\n",ipchar);
		printf("Local Ip in hex:%x\n",mysin.sin_addr.s_addr);

		// Compute the P2P_ID
		// Construct the unsigned char* string to pass into calc_p2p_id

		struct input * p2pinput = (struct input *) malloc (sizeof(struct input));
		p2pinput->ip = nonlocalip;
		p2pinput->port = ntohs(myport);
		p2pinput->lastbyte = 0x0;

		p2p_id = calc_p2p_id((unsigned char*)p2pinput);

		printf("My p2p_id:%d\n",p2p_id);

		free(p2pinput);

		// Handling P2P Read when server starts
		FILE * file = fopen("peers.lst","r+");
		if(file){
				// Initialize myport,p2p_id,L,S,LS,SL
				int p2ptemp;
				char ipchartemp[20];
				int porttemp;
				int found = 0;

				// Move the cursor back
				rewind(file);

				while(fscanf(file,"%d%s%d",&p2ptemp,ipchartemp,&porttemp)!= EOF){
						if (p2ptemp == p2p_id){
								if (strcmp(ipchartemp,ipchar)!=0 || porttemp != myport){
										printf("Conflicts\n");
										exit(0);
								} else {
										found = 1;
										break;
								}
						}
				}

				if(!found){
						fseek(file,0,SEEK_END);
						fprintf(file,"%d %s %d\n", p2p_id,ipchar,myport);
				}

				// Find
				serverInstance ** results = findPredSucc(p2p_id);
				printf("out of findPredSucc\n");
				pred_si = results[0];
				succ_si = results[1];

				// ESTABLISH CONNECTIONS WITH PRED AND SUCC
				// case 1: pred == succ

				if (pred_si){
						printf("inside pred_si if statment\n");
						if(pred_si->p2p_id == succ_si->p2p_id){ 	//THERE IS ONLY 1 other server
								pred_sock = socket(AF_INET, SOCK_STREAM, 0);
								printf("predsock: %d\n",pred_sock);
								if(pred_sock < 0){ perror("socket() faild"); abort(); }

								pred_sin.sin_family = AF_INET;
								pred_sin.sin_addr.s_addr = inet_addr(pred_si->ip);
								pred_sin.sin_port = htons(pred_si->port);

								if(connect(pred_sock,(struct sockaddr *) &pred_sin, sizeof(pred_sin)) < 0){
										//TODO: Handle Disconnection of predecessor
										perror("client - connect");
										close(pred_sock);
										abort();
								}

								// CONNECTED!
								printf("CONNECTED!!!\n");
								handle_sendjoin(pred_sock,p2p_id);
								printf("JOIN IS DONE\n");

						}else{
								pred_sock = socket(AF_INET, SOCK_STREAM, 0);
								if(pred_sock < 0){ perror("socket() faild"); abort(); }

								pred_sin.sin_family = AF_INET;
								pred_sin.sin_addr.s_addr = inet_addr(pred_si->ip);
								pred_sin.sin_port = htons(pred_si->port);

								if(connect(pred_sock,(struct sockaddr *) &pred_sin, sizeof(pred_sin)) < 0){
										//TODO: Handle Disconnection of predecessor
										perror("client - connect");
										close(pred_sock);
										abort();
								}
								handle_sendjoin(pred_sock,p2p_id);
								succ_sock = socket(AF_INET, SOCK_STREAM, 0);
								if(succ_sock < 0){ perror("socket() faild"); abort(); }
								succ_sin.sin_family = AF_INET;
								succ_sin.sin_addr.s_addr = inet_addr(succ_si->ip);
								succ_sin.sin_port = htons(succ_si->ip);
								if(connect(succ_sock,(struct sockaddr *) &succ_sin, sizeof(succ_sin)) < 0){
										//TODO: Handle Disconnection of successocr
										perror("client - connect");
										close(succ_sock); abort();
								}
								// Sending
								handle_sendjoin(succ_sock,p2p_id);
						}
				}


		} else {
				// First server
				file = fopen("peers.lst","w+");
				fprintf(file,"%d %s %d\n", p2p_id,ipchar,myport);
		}
		fclose(file);

		mkdir(DIR,0700);
		chdir(DIR);

		// Creating sockets
		listener = socket(AF_INET, SOCK_STREAM, 0);
		udplistener = socket(AF_INET, SOCK_DGRAM, 0);
		if(listener < 0 || udplistener < 0){perror("socket() failed\n"); abort();}

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin.sin_port = htons(myport);

		udpsin.sin_family = AF_INET;
		udpsin.sin_addr.s_addr = INADDR_ANY;
		udpsin.sin_port = htons(myudpport);

		// Reused
		int optval = 1;
		if (setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval)) < 0){ perror("Reuse failed"); abort(); }
		if (setsockopt(udplistener,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval)) < 0){ perror("Reuse failed"); abort();}

		// Binding
		if (bind(listener,(struct sockaddr *) &sin, sizeof(sin)) < 0){perror("Bind failed");abort();}
		if (bind(udplistener,(struct sockaddr *) &udpsin, sizeof(udpsin)) < 0){perror("UDP Bind failed");abort();}

		if (listen(listener,MAX_CONNECTION)){
				perror("listen");
				abort();
		}

		FD_ZERO(&master);
		FD_ZERO(&readfds);
		FD_ZERO(&login);

		FD_SET(listener,&master);
		FD_SET(udplistener,&master);

		if(udplistener>fdmax) fdmax = udplistener;
		if(listener>fdmax) fdmax = listener;

		int timeout = 1;
		time_t lasttime = time(NULL);

		while(1){ // main accept() lo
				time_t currenttime = time(NULL);
				tv.tv_sec = 5;
				tv.tv_usec = 0;
				timeout = 1;

				readfds = master; // copy it
				if (select(fdmax+1,&readfds,NULL,NULL,&tv) == -1){
						perror("select");
						exit(-1);
				}
				// run through the existing connections looking for data to read
				int i;
				for(i=0; i<= fdmax; i++){
						if (FD_ISSET(i,&readfds)){
								if (currenttime-lasttime < 5) timeout = 0;

								if (i==udplistener){ 
										unsigned char udp_read_buffer[4096];
										int expected_data_len = sizeof(udp_read_buffer);
										int read_bytes = recvfrom(udplistener,
														udp_read_buffer,
														expected_data_len,
														0,
														(struct sockaddr*)&udpsin,&sin_len);

										if(read_bytes < 0){
												perror("Server - Recvfrom Failed - read_bytes return -1\n");
												close(i); // bye!
										} else { // read succedded, proceed

												char msgtype = udp_read_buffer[0];
												unsigned int ip = udpsin.sin_addr.s_addr;
												unsigned int id = (udp_read_buffer[4]<<24)+(udp_read_buffer[3]<<16)+(udp_read_buffer[2]<<8)+udp_read_buffer[1];
												int dup = findDup(mr_array,id,ip); // return the index of the duplicate message

												// ON DUPLICATE
												if(dup >= 0){
														on_udp_duplicate(htonl(ip));
														// resend message player state response
														if(msgtype==PLAYER_STATE_RESPONSE){
																sendto(i,mr_array[dup]->message,
																				PLAYER_STATE_RESPONSE_SIZE,
																				0,
																				(struct sockaddr*)&udpsin,
																				sizeof(udpsin));
																// resend message save state response
														}else if(msgtype==SAVE_STATE_RESPONSE){
																sendto(i,mr_array[dup]->message,
																				SAVE_STATE_RESPONSE_SIZE,
																				0,
																				(struct sockaddr*)&udpsin,
																				sizeof(udpsin));
														}
												}else{	// NOT A DUPLICATE
														if(udp_read_buffer[0] == PLAYER_STATE_REQUEST){
																if(read_bytes != PLAYER_STATE_REQUEST_SIZE){
																		on_malformed_udp(1);
																}else{
																		struct player_state_request * psr = (struct player_state_request *) udp_read_buffer;
																		//check name
																		if(check_player_name(psr->name)==0){	
																				printf("PLAYER NAME IS BAD\n");
																				continue;
																		}
																		process_psr(psr->name,udplistener,udpsin,psr->id,oldest,mr_array,badMessageTypeFlag);
																}


														} else if (udp_read_buffer[0] == SAVE_STATE_REQUEST){

																if(read_bytes != SAVE_STATE_REQUEST_SIZE){
																		on_malformed_udp(1);
																}else{
																		struct save_state_request * ssr = (struct save_state_request *) udp_read_buffer;
																		process_ss_request(ssr->name,
																						ssr->hp,
																						ssr->exp,
																						ssr->x,
																						ssr->y,
																						udplistener,
																						udpsin,ssr->id,
																						oldest,
																						mr_array,
																						faultyErrorCodeFlag);
																}
														}else{
																on_malformed_udp(3);
																continue;
														}
														//	Update Duplicate message check
														oldest = (oldest + 1)%MAX_MESSAGE_RECORD;

												}
										}

										/*
										 *	Handling Data from TCP PORT
										 */ 

								} else if (i==listener){ 
										printf("Received a new tcp connection\n");
										// handle new connection
										int addr_len = sizeof(client_sin);
										int newfd = accept(listener,(struct sockaddr*) &client_sin,&addr_len);
										if (newfd < 0){
												perror("accept failed");
										} else {
												FD_SET(newfd,&master);
												if (newfd > fdmax) fdmax = newfd;
												printf("New connection in socket %d\n", newfd);
										}

								} else {
										unsigned char read_buffer[4096];
										int expected_data_len = sizeof(read_buffer);
										unsigned char *p = (char*) read_buffer; // Introduce a new pointer
										int offset = 0;
										bufferdata * bufferd = fdbuffermap[i];
										unsigned char header_c[HEADER_LENGTH];
										struct header * hdr;

										int read_bytes = recv(i,read_buffer,expected_data_len, 0);

										if (read_bytes <= 0){ 
												close(i); // bye!
												FD_CLR(i,&login);
												FD_CLR(i,&master); // remove from the master set

												// got error or connection closed by client
												if(read_bytes <= 0){
														printf("Socket %d hung up\n",i);
														if (fdnamemap[i]){
																Player * player = findPlayer(fdnamemap[i],primarylist);
																if(player){
																		unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
																		createlogoutnotify(fdnamemap[i],lntosent);
																		broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
																		removePlayer(fdnamemap[i],primarylist);
																}
														}
												}
												cleanNameMap(fdnamemap,i);
												cleanBuffer(fdbuffermap,i);

										} else {
												if(bufferd->buffer == NULL){
														bufferd->buffer = (char*)malloc(sizeof(char)*read_bytes);
														memcpy(bufferd->buffer,read_buffer,read_bytes);
												} else {
														bufferd->buffer = (char*)realloc(bufferd->buffer,bufferd->buffer_size+read_bytes);
														memcpy(bufferd->buffer+bufferd->buffer_size,read_buffer,read_bytes);
												}
												bufferd->buffer_size += read_bytes;
												while (bufferd->buffer_size >= bufferd->desire_length){
														if(bufferd->flag == HEADER){
																int j;
																for(j = 0; j < HEADER_LENGTH; j++){ header_c[j] = *(bufferd->buffer+j);}
																hdr = (struct header *) header_c;
																if (check_malformed_header(hdr->version,ntohs(hdr->len),hdr->msgtype) < 0){
																		close(i);
																		FD_CLR(i,&login);
																		FD_CLR(i,&master);
																		if(fdnamemap[i]){
																				Player * player = findPlayer(fdnamemap[i],primarylist);
																				if(player){ unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
																						createlogoutnotify(player,lntosent);
																						broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
																						removePlayer(fdnamemap[i],primarylist);
																				}
																				cleanNameMap(fdnamemap,i);
																		}
																		if(fdbuffermap) cleanBuffer(fdbuffermap,i);
																		break;
																} else { // If not malform
																		char * temp = (char*) malloc(sizeof(char)*(bufferd->buffer_size-HEADER_LENGTH));
																		memcpy(temp,bufferd->buffer+4,bufferd->buffer_size-4);
																		free(bufferd->buffer);
																		bufferd->buffer = temp;
																		bufferd->buffer_size -= HEADER_LENGTH;
																		bufferd->desire_length = ntohs(hdr->len)-HEADER_LENGTH;
																		bufferd->flag = PAYLOAD;
																}

														} else { // Payload

																char payload_c[bufferd->desire_length];
																int j;
																for(j = 0; j < bufferd->desire_length; j++){ payload_c[j] = *(bufferd->buffer+j);}

																if(hdr->msgtype == LOGIN_REQUEST){ // LOGIN REQUEST
																		if (FD_ISSET(i,&login)){ // Is he typing in login again?
																				unsigned char ivstate[8];
																				createinvalidstate(1,ivstate);
																				int bytes_sent = send(i, ivstate,INVALID_STATE_SIZE,0);
																				if (bytes_sent < 0){
																						perror("send failed");
																						abort();
																				}
																		} else { // If he is not logged in

																				//if(check_malformed_stats(ssr->x,ssr->y,ntohl(ssr->hp),ntohl(ssr->exp))!=0){
																				struct login_request * lr = (struct login_request *) payload_c;
																				if(check_player_name(lr->name)==0 || check_malformed_stats(lr->x,lr->y,ntohl(lr->hp),ntohl(lr->exp))!=0){
																						close(i);
																						FD_CLR(i,&login);
																						FD_CLR(i,&master);

																						Player * player = findPlayer(lr->name,primarylist);
																						if(player){
																								// Broadcasting the logout notify to other client
																								unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
																								createlogoutnotify(player,lntosent);
																								broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
																								removePlayer(fdnamemap[i],primarylist);
																								cleanNameMap(fdnamemap,i);
																						}
																						cleanBuffer(fdbuffermap,i);
																						break;
																				}else if (isnameinmap(lr->name,fdnamemap)){ // If the name is already used
																						// Send a login request with an errorcode 1
																						Player * newplayer = process_login_request(1,
																										i,
																										fdmax,
																										login,
																										lr->name,
																										lr->hp,
																										lr->exp,
																										lr->x,
																										lr->y,primarylist);
																				} else {
																						FD_SET(i,&login); // Log him in
																						Player * newplayer = process_login_request(0,i,fdmax,login,lr->name,lr->hp,lr->exp,lr->x,lr->y,primarylist);
																						if (!fdnamemap[i]){ fdnamemap[i] = malloc(sizeof(char)*11);}
																						strcpy(fdnamemap[i],lr->name);
																						strcpy(newplayer->name,fdnamemap[i]);

																						// Adding the player
																						Node * node = (Node*) malloc(sizeof(Node)); // TODO: remember to free this
																						node->datum = newplayer;
																						node->next = NULL;
																						if(primarylist->head == NULL){
																								primarylist->head = node;
																								primarylist->tail = node;
																						}else {
																								primarylist->tail->next = node;
																								primarylist->tail = node;
																						}
																				}
																		}

																		} else if(hdr->msgtype == MOVE){ // MOVE
																				if (!FD_ISSET(i,&login)){ // if not login,

																						// Send invalid state
																						unsigned char ivstate[INVALID_STATE_SIZE];
																						createinvalidstate(0,ivstate);
																						int bytes_sent = send(i, ivstate,INVALID_STATE_SIZE,0);
																						if (bytes_sent < 0){
																								perror("send failed");
																								abort();
																						}

																				} else { // If logged in, good to proceed
																						struct move * m = (struct move *) payload_c;
																						int direction = m->direction;
																						Player * player;                     
																						if (fdnamemap[i]) { player = findPlayer(fdnamemap[i],primarylist);
																						} else {
																								fprintf(stderr, "THIS SHOULD NEVER HAPPEN\n");
																						}
																						if(player){
																								stats(player);
																								if(direction==NORTH){ player->y -= 3; player->y = (100+player->y) % 100;
																								}else if(direction==SOUTH){ player->y += 3; player->y = (100+player->y) % 100;
																								}else if(direction==WEST){ player->x -= 3; player->x = (100+player->x) % 100;
																								}else if(direction==EAST){ player->x += 3; player->x = (100+player->x) % 100;
																								}else {
																										close(i);
																										FD_CLR(i,&login);
																										FD_CLR(i,&master);
																										if(fdnamemap[i]){
																												Player * player = findPlayer(fdnamemap[i],primarylist);
																												if(player){

																														// Broadcast to other clients
																														unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
																														createlogoutnotify(player,lntosent);
																														broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
																														removePlayer(fdnamemap[i],primarylist);

																												} else {
																														fprintf(stderr, "THIS SHOULD NEVER HAPPEN\n");
																														exit(-1);
																												}
																												cleanNameMap(fdnamemap,i);
																										}
																										cleanBuffer(fdbuffermap,i);
																										break;
																								}
																								unsigned char mntosent[MOVE_NOTIFY_SIZE];
																								createmovenotify(fdnamemap[i],player->hp,player->exp,player->x,player->y,mntosent);
																								broadcast(login,i,fdmax,mntosent,MOVE_NOTIFY_SIZE);
																						}
																				}

																		} else if(hdr->msgtype == ATTACK){ // ATTACK
																				if (!FD_ISSET(i,&login)){ // if not login,
																						// Send invalid state
																						unsigned char ivstate[INVALID_STATE_SIZE];
																						createinvalidstate(0,ivstate);
																						int bytes_sent = send(i, ivstate,INVALID_STATE_SIZE,0);
																						if (bytes_sent < 0) perror("send failed"); abort();

																				} else {
																						struct attack * attackPayload = (struct attack *) payload_c;
																						char * victim = attackPayload->victimname;
																						if (check_player_name(victim) == 0){
																								close(i);
																								FD_CLR(i,&login);
																								FD_CLR(i,&master);
																								if(fdnamemap[i]){
																										Player * player = findPlayer(fdnamemap[i],primarylist);
																										if(player){
																												unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
																												createlogoutnotify(player,lntosent);
																												broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
																												removePlayer(fdnamemap[i],primarylist);
																										} else {
																												fprintf(stderr, "THIS SHOULD NEVER HAPPEN\n");
																												exit(-1);
																										}
																										cleanNameMap(fdnamemap,i);
																								}
																								cleanBuffer(fdbuffermap,i);
																								break;
																						}
																						char * attacker;
																						if (fdnamemap[i])attacker = fdnamemap[i];
																						else fprintf(stderr,"server.c - attack - THIS SHOULD NEVER HAPPEN.\n");
																						process_attack(i,fdmax,login,attacker,victim,primarylist);
																				}
																		} else if(hdr->msgtype == SPEAK){ // SPEAK
																				if (!FD_ISSET(i,&login)){ // if not login,
																						// Send invalid state
																						unsigned char ivstate[INVALID_STATE_SIZE];
																						createinvalidstate(0,ivstate);
																						int bytes_sent = send(i, ivstate,INVALID_STATE_SIZE,0);
																						if (bytes_sent < 0){ perror("send failed"); abort();}
																				} else {
																						struct speak * speakPayload = (struct speak *) payload_c;

																						// Check for malformed message
																						if(check_player_message(payload_c)==0){
																								close(i);
																								FD_CLR(i,&login);
																								FD_CLR(i,&master);
																								if(fdnamemap[i]){
																										Player * player = findPlayer(fdnamemap[i],primarylist);
																										if(player){

																												unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
																												createlogoutnotify(player,lntosent);
																												broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
																												removePlayer(fdnamemap[i],primarylist);

																										} else {
																												fprintf(stderr,"Internal data structure error\n");
																										}
																										cleanNameMap(fdnamemap,i);
																								}
																								cleanBuffer(fdbuffermap,i);
																								break;
																						}
																						int msglen = strlen(payload_c)+1+10+HEADER_LENGTH;
																						int totallen;
																						if(msglen%4) totallen = msglen + (4 - msglen%4);
																						else totallen = msglen;
																						unsigned char spktosent[totallen];
																						createspeaknotify(fdnamemap[i],payload_c,totallen,spktosent);
																						printMessage(spktosent,totallen);
																						broadcast(login,i,fdmax,spktosent,totallen);
																				}
																		} else if(hdr->msgtype == LOGOUT){
																				if (!FD_ISSET(i,&login)){ // if not login,
																						unsigned char ivstate[INVALID_STATE_SIZE];
																						createinvalidstate(0,ivstate);
																						int bytes_sent = send(i, ivstate,INVALID_STATE_SIZE,0);
																						if (bytes_sent < 0){
																								perror("send failed");
																								abort();
																						}
																				} else {  // Player is logged in
																						if(fdnamemap[i]){
																								Player * player = findPlayer(fdnamemap[i],primarylist);
																								if(player){
																										unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
																										createlogoutnotify(player,lntosent);
																										broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
																										removePlayer(fdnamemap[i],primarylist);
																										close(i);
																										FD_CLR(i,&login);
																										FD_CLR(i,&master);
																										cleanNameMap(fdnamemap,i);
																								} else {
																										fprintf(stderr, "Internal data structure error");
																								}
																						}
																						cleanBuffer(fdbuffermap,i);
																						bufferd = fdbuffermap[i];
																						break;
																				}
																		} else if(hdr->msgtype == P2P_JOIN_REQUEST){
																				struct p2p_join_request * jr_payload = (struct p2p_join_request *) payload_c;

																				serverInstance **result = findPredSucc(p2p_id);
																				serverInstance *pred_si = result[0];
																				serverInstance *succ_si = result[1];

																				unsigned int requestp2p_id = ntohl(jr_payload->p2p_id);

																				if( pred_si->p2p_id == requestp2p_id ){
																						// Handling request from pred

																				}else if ( succ_si->p2p_id == requestp2p_id ){
																						// Handling request from succ
																						// Modify its primary and backup range

																						// primary->high and backup->low stay the same
																						primary->low = requestp2p_id+1;
																						backup->high = requestp2p_id;

																						int count = 0;
																						Node * p;

																						// For all the player in the primary list
																						// First pass: Count how many
																						for(p = primarylist->head; p; p=p->next){
																								Player  * curp = p->datum;
																								// requestor->primary = our backup
																								if(curp->p2p_id >= backup->low && curp->p2p_id <= backup->high){
																										count++;
																								}
																						}

																						// Second pass: allocate memory and populate the array
																						unsigned char * userdata = (unsigned char *) malloc (sizeof(char)*count*20);
																						unsigned int offset = 0;
																						for(p = primarylist->head; p; p=p->next){
																								Player  * curp = p->datum;
																								// requestor->primary = our backup
																								if(curp->p2p_id >= backup->low && curp->p2p_id <= backup->high){ // If in range
																										memcpy(userdata+offset,curp->name,10); offset += 10;
																										memcpy(userdata+offset,curp->hp,4);    offset += 4;
																										memcpy(userdata+offset,curp->exp,4);   offset += 4;
																										memcpy(userdata+offset,curp->x,1);     offset += 1;
																										memcpy(userdata+offset,curp->y,1);     offset += 1;
																								}
																						}
																						if (offset != count*20){
																								perror("Something wrong happens when copying to userdata");
																								exit(0);
																						}
																						handle_sendjoinresponse(i,count,userdata);

																						// No need for this anymore
																						free(userdata);
																				}else{
																						// malformed
																				}



																		} else if(hdr->msgtype == P2P_JOIN_RESPONSE){
																				/*
																				   Populate the data
																				   */
																				struct p2p_join_response * jr_payload = (struct p2p_join_response *) payload_c;
																				unsigned int total = ntohl(jr_payload->usernumber);
																				unsigned char * userdata = jr_payload+4;

																				int i;
																				for(i=0; i<total; i++){
																						Player * player = constructPlayer(userdata+i*20);
																						// Assuming that backup and primary are initialized
																						if(isInRange(player->p2p_id,backup)){
																								addPlayer(backuplist,player);

																						} else if(isInRange(player->p2p_id,primary)){
																								addPlayer(primarylist,player);

																						} else {
																								printf("This player is not in my range!\n");
																						}
																				}
																		} else if(hdr->msgtype == P2P_BKUP_REQUEST){
																		} else if(hdr->msgtype == P2P_BKUP_RESPONSE){
																		} else {
																				printf("We got nothing");
																		}
																		// Move the pointers
																		char * temp = (char*) malloc(sizeof(char)*(bufferd->buffer_size-bufferd->desire_length));
																		memcpy(temp,
																						bufferd->buffer+bufferd->desire_length,
																						bufferd->buffer_size-bufferd->desire_length);
																		free(bufferd->buffer);
																		bufferd->buffer = temp;
																		bufferd->buffer_size -= bufferd->desire_length;
																		bufferd->desire_length = HEADER_LENGTH;
																		bufferd->flag = HEADER;
																} // end of handling payload
														} // End of while more desired length
												} // end else
										}  // end else someone has data
								} // end FD_ISSET
						}  // end foreach fd
						if (timeout){
								//updateHP(primarylist);
								lasttime = currenttime;
						}
				} // end main while(1) loop

		}
