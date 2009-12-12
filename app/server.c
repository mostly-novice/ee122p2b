// Server code
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <dirent.h>

#include "header.h"
#include "messages.h"
#include "server_output.h"
#include "constants.h"

#define STDIN 0
#define HEADER_LENGTH 4
#define USERDIR "users"

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

void cleanNameMap(char ** fdnamemap,int i){
  printf("Cleaning up name map \n");
  if(fdnamemap[i]){
    free(fdnamemap[i]);
    fdnamemap[i] = NULL;
  }
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

void printMessage(char * message, int len){
  int i;
  for(i = 0; i < len; i++){
    printf("%02x ", *(message+i));
  }
  printf("\n");
}


#include "model.h"
#include "processHelper.h"
#include "aux.h"
#include "p2p.h"

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

void loadData(LinkedList * list,Range * range){
  char currentPath[300];
  chdir("users");
  getwd(currentPath);
  struct dirent *ep;
  FILE * file;

  DIR * dp = opendir ("./");
  if (dp != NULL) {
    while (ep = readdir (dp)){
      if (strcmp(ep->d_name,".")!=0 &&strcmp(ep->d_name,"..")!=0){
	int p2p_id = calc_p2p_id(ep->d_name);
	if(isInRange(p2p_id,range)){
	  int hp;
	  int exp;
	  int x;
	  int y;
	  Player * player = (Player*) malloc (sizeof(Player));
	  file = fopen(ep->d_name,"r");
	  if(file){
	    fscanf(file,"%d%d%d%d",&hp,&exp,&x,&y);
	    fclose(file);
	    strcpy(player->name,ep->d_name);
	    player->name[strlen(ep->d_name)] = '\0';
	    player->p2p_id = calc_p2p_id(player->name);
	    player->hp  = hp;
	    player->exp = exp;
	    player->x   = x;
	    player->y   = y;
	    player->p2p_id = p2p_id;

	    addPlayer(player,list);
	    /* fprintf(stdout,"loadData: add player %s (HP=%d,EXP=%d,X=%d,Y=%d,P2P_ID=%d)\n", */
/* 		    player->name, */
/* 		    player->hp, */
/* 		    player->exp, */
/* 		    player->x, */
/* 		    player->y, */
/* 		    player->p2p_id); */
	  } else {
	    printf("File %s is not found\n",ep->d_name);
	  }
	}
      }
    }
    closedir(dp);
  } else {
    chdir("..");
    perror ("Couldn't open the directory");
    exit(-1);
  }
  chdir("..");
  return 0;
}


int main(int argc, char* argv[]){

  /*
    These are pointer to serverInstance for predecessor and successor
  */

  serverInstance * predecessor_si;
  serverInstance * successor_si;

  int pred_sock = -1;
  int succ_sock = -1;

  Range * primary = (Range *) malloc (sizeof(Range));
  Range * backup  = (Range *) malloc (sizeof(Range));

  primary->low = 0;
  primary->high = 1023;

  backup->low = 0;
  backup->high = 1023;

  unsigned int p2p_id; // [0-1023]

  // Model Variables
  LinkedList * mylist = (LinkedList *) malloc (sizeof(LinkedList));
  mylist->head = NULL;
  mylist->tail = NULL;

  // P2P: Primary List and Backup List
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

  int listener,udplistener;
  int myport,myudpport;
  int done = 0;
  int status;

  int closepredflag = 0;
  int tobeclose = -1;

  message_record ** mr_array = malloc(sizeof(*mr_array)*MAX_MESSAGE_RECORD);

  fd_set master,readfds,login;

  // Clearing out the set
  FD_ZERO(&master);
  FD_ZERO(&readfds);
  FD_ZERO(&login);

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

  struct sockaddr_in guestsin;
  memset(&guestsin, 0, sizeof(guestsin));

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


  /*
    P2P: Server Starts
  */

  // Find the nonlocal_ip
  char hostname[30];
  gethostname(hostname,30);
  struct hostent * hp = gethostbyname(hostname);
  struct sockaddr_in mysin;
  char * ipchar = inet_ntoa(*(struct in_addr*)(hp->h_addr_list[0]));
  inet_aton(ipchar,&mysin.sin_addr);
  int nonlocalip = mysin.sin_addr.s_addr;

  // Calculating the P2P_ID
  struct input * p2pinput = (struct input *) malloc (sizeof(struct input));
  p2pinput->ip = nonlocalip;
  p2pinput->port = ntohs(myport);
  p2pinput->lastbyte = 0x0;

  p2p_id = calc_p2p_id((unsigned char*)p2pinput);
  free(p2pinput);

  fprintf(stdout,"P2P: My p2p_id:%d\n",p2p_id);

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
	  exit(-1);
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
    fclose(file);

    // Find
    serverInstance ** results = findPredSucc(p2p_id);
    serverInstance * pred_si = results[0];
    serverInstance * succ_si = results[1];

    predecessor_si = pred_si;
    successor_si = succ_si;

    printf("P2P: pred: %d succ: %d\n",pred_si->p2p_id,succ_si->p2p_id);

    // ESTABLISH CONNECTIONS WITH PRED AND SUCC
    // case 1: pred == succ
    if (pred_si){
      if(pred_si->p2p_id == succ_si->p2p_id){ 	//THERE IS ONLY 1 other server

	// Create a new connection
	newconnection(pred_si->ip,pred_si->port,&pred_sock);

	succ_sock = pred_sock;

	fprintf(stdout,"P2P: send P2P_JOIN_REQUEST to pred %d\n",pred_si->p2p_id);
	handle_sendjoin(pred_sock,p2p_id);

	FD_SET(pred_sock,&master); // Adding pred_sock to the master list
	fdmax = max(fdmax,pred_sock);

	// Change the primary and backup range
	primary->low = pred_si->p2p_id+1;
	primary->high = p2p_id;

	//fprintf(stdout,"P2P: Primary range - [%d-%d]\n",primary->low,primary->high);

	backup->low = p2p_id+1;
	backup->high = pred_si->p2p_id;

	//fprintf(stdout,"P2P: Backup range - [%d-%d]\n",backup->low,backup->high);

      }else{
	printf("P2P: %d => %d => %d\n",pred_si->p2p_id,p2p_id,succ_si->p2p_id);

	newconnection(pred_si->ip,pred_si->port,&pred_sock);
	fprintf(stdout,"P2P: send P2P_JOIN_REQUEST to pred %d\n",pred_si->p2p_id);
	handle_sendjoin(pred_sock,p2p_id);
	FD_SET(pred_sock,&master);
	fdmax = max(fdmax,pred_sock);

	tobeclose = pred_sock;
	closepredflag = 1;

	// Change the primary and backup range
	primary->low = pred_si->p2p_id+1;
	primary->high = p2p_id;

	//fprintf(stdout,"P2P: Primary range - [%d-%d]\n",primary->low,primary->high);

	backup->low = findPred(pred_si->p2p_id)+1;
	backup->high = pred_si->p2p_id;

	//fprintf(stdout,"P2P: Backup range - [%d-%d]\n",backup->low,backup->high);
      }
    }


  } else {
    // First server
    printf("P2P: I m the first server.\n");
    //printf("Loading data...\n");
    //printf("Loading to primary.\n");
    loadData(primarylist,primary);
    //printf("Loading to backup.\n");
    loadData(backuplist,backup);
    file = fopen("peers.lst","w+");
    fprintf(file,"%d %s %d\n", p2p_id,ipchar,myport);
    fclose(file);
  }

  mkdir(USERDIR,0700);

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

  FD_SET(listener,&master);
  FD_SET(udplistener,&master);

  fdmax = max(fdmax,udplistener);
  fdmax = max(fdmax,listener);

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
	  socklen_t addrLen = sizeof(guestsin);
	  int read_bytes = recvfrom(udplistener,
				    udp_read_buffer,
				    expected_data_len,
				    0,
				    (struct sockaddr*)&guestsin,&addrLen);

	  //printf("Got data from %s:%d\n", inet_ntoa(guestsin.sin_addr),guestsin.sin_port);

	  if(read_bytes < 0){
	    perror("Server - Recvfrom Failed - read_bytes return -1\n");
	    close(i); // bye!
	  } else { // read succedded, proceed

	    char msgtype = udp_read_buffer[0];
	    unsigned int ip = guestsin.sin_addr.s_addr;
	    unsigned int id = (udp_read_buffer[4]<<24)+
	      (udp_read_buffer[3]<<16)+
	      (udp_read_buffer[2]<<8)+udp_read_buffer[1];
	    int dup = findDup(mr_array,id,ip); // return the index of the duplicate message

	    // ON DUPLICATE
	    if(dup >= 0){
	      on_udp_duplicate(htonl(ip));
	      // resend message player state response
	      if(msgtype==PLAYER_STATE_RESPONSE){
		sendto(i,mr_array[dup]->message,
		       PLAYER_STATE_RESPONSE_SIZE,
		       0,
		       (struct sockaddr*)&guestsin,
		       sizeof(guestsin));
		// resend message save state response
	      }else if(msgtype==SAVE_STATE_RESPONSE){
		sendto(i,mr_array[dup]->message,
		       SAVE_STATE_RESPONSE_SIZE,
		       0,
		       (struct sockaddr*)&guestsin,
		       sizeof(guestsin));
	      }
	    }else{	// NOT A DUPLICATE
	      if(udp_read_buffer[0] == PLAYER_STATE_REQUEST){
		printf("Handling player state request\n");
		if(read_bytes != PLAYER_STATE_REQUEST_SIZE){
		  on_malformed_udp(1);
		}else{
		  struct player_state_request * psr = (struct player_state_request *) udp_read_buffer;

		  if(check_player_name(psr->name)==0){	
		    printf("PLAYER NAME IS BAD\n");
		    continue;
		  }
		  process_psr(psr->name,udplistener,guestsin,psr->id,oldest,mr_array,badMessageTypeFlag);
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
				     guestsin,ssr->id,
				     oldest,
				     mr_array,
				     faultyErrorCodeFlag);

		  // Send a p2p_bkup_request
		  struct playerpacket * pp = (struct playerpacket *) malloc (sizeof(struct playerpacket));
		  strcpy(pp->name,ssr->name);
		  pp->hp = ssr->hp;
		  pp->exp = ssr->exp;
		  pp->x = ssr->x;
		  pp->y = ssr->y;
		  unsigned char * userdata = (unsigned char *) pp;

		  if (succ_sock != -1){
		    printf("P2P: Send P2P_BKUP_REQUEST to sock %d\n",succ_sock);
		    handle_sendbkuprequest(succ_sock,userdata);
		  }
		}
	      }else{
		printf("Udp_read_buffer[0]: %d\n",udp_read_buffer[0]);
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
	  // handle new connection
	  struct sockaddr_in from;
	  socklen_t from_len = sizeof(from);
	  int newfd = accept(listener,(struct sockaddr*) &from,&from_len);
	  if (newfd < 0){
	    perror("accept failed");
	  } else {
	    FD_SET(newfd,&master);
	    if (newfd > fdmax) fdmax = newfd;
	    printf("New connection in socket %d from %s:%u\n",
		   newfd,
		   inet_ntoa(from.sin_addr),
		   ntohs(from.sin_port)); // If I get a new connection from the predecessor
	    if (closepredflag && tobeclose != -1){
	      printf("Closing previous connection.\n");
	      closepredflag = 0;
	      close(tobeclose);
	      FD_CLR(tobeclose,&master);
	    }
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
	    if(read_bytes <= 0){
	      if(succ_sock == i && pred_sock != succ_sock){
		printf("P2P: Backup server(%s:%d,%d) goes down fd=%d\n",
		       successor_si->ip,
		       successor_si->port,
		       successor_si->p2p_id,
		       succ_sock);
		serverInstance ** results = findPredSucc(successor_si->p2p_id);
		serverInstance * succ_si = results[1];

		if(p2p_id != succ_si->p2p_id){
		  printf("P2P:Connecting to the new backup %s:%d(%d)\n",
			 succ_si->ip,
			 succ_si->port,
			 succ_si->p2p_id);
		  newconnection(succ_si->ip,succ_si->port,&succ_sock);
		  printf("P2P: New socket for successor: %d\n",succ_sock);
		} else {
		  printf("I am the only one left\n");
		}
	      }
	      printf("Socket %d hung up\n",i);
	      if (fdnamemap[i]){
		Player * player = findPlayer(fdnamemap[i],mylist);
		if(player){
		  unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
		  createlogoutnotify(fdnamemap[i],lntosent);
		  broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
		  removePlayer(fdnamemap[i],mylist);
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

		char * temp = (char*) malloc(sizeof(char)*(bufferd->buffer_size-HEADER_LENGTH));
		memcpy(temp,bufferd->buffer+4,bufferd->buffer_size-4);
		free(bufferd->buffer);
		bufferd->buffer = temp;
		bufferd->buffer_size -= HEADER_LENGTH;
		bufferd->desire_length = ntohs(hdr->len)-HEADER_LENGTH;
		bufferd->flag = PAYLOAD;

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
		    struct login_request * lr = (struct login_request *) payload_c;
		    if(check_player_name(lr->name)==0 || check_malformed_stats(lr->x,lr->y,ntohl(lr->hp),ntohl(lr->exp))!=0){
		      close(i);
		      FD_CLR(i,&login);
		      FD_CLR(i,&master);

		      Player * player = findPlayer(lr->name,mylist);
		      if(player){
			// Broadcasting the logout notify to other client
			unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
			createlogoutnotify(player,lntosent);
			broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
			removePlayer(fdnamemap[i],mylist);
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
								 lr->y,mylist);
		    } else {
		      FD_SET(i,&login); // Log him in
		      Player * newplayer = process_login_request(0,
								 i,
								 fdmax,
								 login,
								 lr->name,
								 lr->hp,
								 lr->exp,
								 lr->x,
								 lr->y,
								 mylist);
		      if (!fdnamemap[i]){ fdnamemap[i] = malloc(sizeof(char)*11);}
		      strcpy(fdnamemap[i],lr->name);
		      strcpy(newplayer->name,fdnamemap[i]);

		      // Adding the player
		      Node * node = (Node*) malloc(sizeof(Node)); // TODO: remember to free this
		      node->datum = newplayer;
		      node->next = NULL;
		      if(mylist->head == NULL){
			mylist->head = node;
			mylist->tail = node;
		      }else {
			mylist->tail->next = node;
			mylist->tail = node;
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
		    if (fdnamemap[i]) { player = findPlayer(fdnamemap[i],mylist);
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
			  Player * player = findPlayer(fdnamemap[i],mylist);
			  if(player){

			    // Broadcast to other clients
			    unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
			    createlogoutnotify(player,lntosent);
			    broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
			    removePlayer(fdnamemap[i],mylist);

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
			Player * player = findPlayer(fdnamemap[i],mylist);
			if(player){
			  unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
			  createlogoutnotify(player,lntosent);
			  broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
			  removePlayer(fdnamemap[i],mylist);
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
		    process_attack(i,fdmax,login,attacker,victim,mylist);
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
			Player * player = findPlayer(fdnamemap[i],mylist);
			if(player){
			  unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
			  createlogoutnotify(player,lntosent);
			  broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
			  removePlayer(fdnamemap[i],mylist);

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
		      Player * player = findPlayer(fdnamemap[i],mylist);
		      if(player){
			unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
			createlogoutnotify(player,lntosent);
			broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);
			removePlayer(fdnamemap[i],mylist);
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

		  printf("P2P: recv P2P_JOIN_REQUEST from %d\n",requestp2p_id);

		  // If I am his/her succ
		  if( pred_si->p2p_id == requestp2p_id ){
		    predecessor_si = pred_si;
		    printf("%d is my predecessor\n",requestp2p_id);
		    primary->low  = requestp2p_id+1;
		    primary->high = p2p_id;
		    backup->low   = findPred(requestp2p_id)+1;
		    backup->high  = requestp2p_id;


		    fprintf(stdout,"My new range is: Primary = [%d - %d], Backup = [%d - %d].\n",
			    primary->low,
			    primary->high,
			    backup->low,
			    backup->high);
		    int count = 0;
		    Node * p;
		    // For all the player in the primary list
		    // First pass: Count how many
		    for(p = primarylist->head; p; p=p->next){
		      Player  * curp = p->datum;
		      if(isInRange(curp->p2p_id,backup)){
			count++;
		      }
		    }
		    // Second pass: allocate memory and populate the array
		    unsigned char userdata[20*count];
		    memset(userdata,0,count*20);
		    unsigned int offset = 0;
		    for(p = primarylist->head; p; p=p->next){
		      Player  * curp = p->datum;
		      // requestor->primary = our backup
		      if(isInRange(curp->p2p_id,backup)){ // If in range
			struct playerpacket * pp = (struct playerpacket *) malloc (sizeof(struct playerpacket));

			strcpy(pp->name,curp->name);
			pp->hp  = htonl(curp->hp);
			pp->exp = htonl(curp->exp);
			pp->x   = curp->x;
			pp->y   = curp->y;

			unsigned char * ppchar = (unsigned char *) pp;
			memcpy(userdata+offset,ppchar,20);
			offset+=20;
			free(pp);
		      }
		    }
		    printf("P2P: send P2P_JOIN_RESPONSE to pred %d (%d users)\n",requestp2p_id,count);

		    handle_sendjoinresponse(i,count,userdata);

		    printf("P2P: Setting pred_sock to %d\n",i);
		    pred_sock = i;
		  }


		  // If I am his/her pred
		  if ( succ_si->p2p_id == requestp2p_id ){
		    printf("%d is my successor\n",requestp2p_id);
		    int count = 0;
		    Node * p;
		    for(p = primarylist->head; p; p=p->next){
		      Player * curp = p->datum;
		      if(isInRange(curp->p2p_id,primary)){
			count++;
		      }
		    }

		    unsigned char userdata2[20*count];
		    memset(userdata2,0,count*20);
		    unsigned int offset = 0;
		    for(p = primarylist->head; p; p=p->next){
		      Player  * curp = p->datum;
		      if(isInRange(curp->p2p_id,primary)){
			unsigned char pptemp[20];
			struct playerpacket * pp = (struct playerpacket *) pptemp;

			strcpy(pp->name,curp->name);
			pp->hp  = htonl(curp->hp);
			pp->exp = htonl(curp->exp);
			pp->x   = curp->x;
			pp->y   = curp->y;

			unsigned char * ppchar = (unsigned char *) pp;
			memcpy(userdata2+offset,ppchar,20);
			offset+=20;
		      }
		    }

		    printf("P2P: Send JOIN_REPONSE to sock: %d (%d users)\n",i,count);
		    handle_sendjoinresponse(i,count,userdata2);

		    // Close connection to the previous successor
		    if (succ_sock != -1){
		      printf("Close existing successor's connection fd=%d %s:%d(%d)\n",
			     succ_sock,
			     successor_si->ip,
			     successor_si->port,
			     successor_si->p2p_id);
		      close(succ_sock);
		      FD_CLR(succ_sock,&master);
		    }

		    sleep(1);

		    // Make a new connection
		    newconnection(succ_si->ip,succ_si->port,&succ_sock);
		    printf("P2P: Connected to fd=%d,%s:%d(%d)\n",succ_sock,succ_si->ip,succ_si->port,succ_si->p2p_id);
		    //printf("P2P: Setting succ_sock to %d\n",succ_sock);

		    successor_si = succ_si;
		  }


		} else if(hdr->msgtype == P2P_JOIN_RESPONSE){

		  struct p2p_join_response * jr_payload = (struct p2p_join_response *) payload_c;
		  unsigned int total = ntohl(jr_payload->usernumber);
		  unsigned char * userdata = payload_c+4;

		  printf("P2P: recv P2P_JOIN_RESPONSE (%d users)\n",total);

		  int index;
		  for(index=0; index<total; index++){
		    Player * player = constructPlayer(userdata,index*20);
		    // Assuming that backup and primary are initialized
		    if(isInRange(player->p2p_id,backup)){
		      /* fprintf(stdout,"P2P: add player %s (HP=%d,EXP=%d,X=%d,Y=%d,P2P_ID=%d) to backup\n", */
/* 			      player->name, */
/* 			      player->hp, */
/* 			      player->exp, */
/* 			      player->x, */
/* 			      player->y, */
/* 			      player->p2p_id); */
		      addPlayer(player,backuplist);

		    } else if(isInRange(player->p2p_id,primary)){
		      /* fprintf(stdout,"P2P: add player %s (HP=%d,EXP=%d,X=%d,Y=%d,P2P_ID=%d) to primary\n", */
/* 			      player->name, */
/* 			      player->hp, */
/* 			      player->exp, */
/* 			      player->x, */
/* 			      player->y, */
/* 			      player->p2p_id); */
		      addPlayer(player,primarylist);

		    } else {

		      printf("This player:%d(name: %s) is not in my ranges!\n",player->p2p_id,player->name);
		    }

		    if (i == pred_sock && pred_sock != succ_sock){

		      printf("P2P: newguy -close existing connection on fd=%d(%d).\n",pred_sock,predecessor_si->p2p_id);
		      FD_CLR(i,&master);
		      pred_sock = -1;
		      close(i);

		      sleep(1);

		      newconnection(successor_si->ip,successor_si->port,&succ_sock);
		      fprintf(stdout,"P2P: send P2P_JOIN_REQUEST to succ %d\n",successor_si->p2p_id);
		      handle_sendjoin(succ_sock,p2p_id);
		      FD_SET(succ_sock,&master);
		      fdmax = max(fdmax,succ_sock);
		    }
		  }
		} else if(hdr->msgtype == P2P_BKUP_REQUEST){
		  printf("P2P: Recieved BKUP_REQUEST\n");
		  struct p2p_bkup_request * br_payload = (struct p2p_bkup_request *) payload_c;
		  struct playerpacket *pp = (struct playerpacket *) br_payload->userdata;

		  //FIND the player (if exists) and update his stats, otherwise add player
		  int found = 0;
		  Node *p;
		  for(p = backuplist->head; p; p=p->next){
		    Player * curp = p->datum;
		    if(strcmp(curp->name,pp->name)==0){
		      found = 1;
		      curp->x = pp->x;
		      curp->y = pp->y;
		      curp->exp = ntohl(pp->exp);
		      curp->hp = ntohl(pp->hp);
		    }
		  }

		  if(!found){
		    printf("%s is a new player. Adding him/her now.\n",pp->name);
			Player* p = (Player *)malloc(sizeof(Player));
			strcpy(p->name,pp->name);
			p->x = pp->x;
			p->y = pp->y;
			p->exp = pp->exp;
			p->hp = pp->hp;
		    addPlayer(p,primarylist);

		  }

		  handle_sendbkupresponse(i,0);
		} else if(hdr->msgtype == P2P_BKUP_RESPONSE){
		  struct p2p_bkup_request * br_payload = (struct p2p_bkup_request *) payload_c;
		  printf("WE GOT A BKUP_RESPONSE\n");
		} else {
		  printf("hdr->msgtype:%d\n",hdr->msgtype);
		  printf("We got nothing\n");
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
      //updateHP(mylist);
      lasttime = currenttime;
    }
  } // end main while(1) loop
}
