#include <string.h>
#include <errno.h>

unsigned int calc_p2p_id(unsigned char *s){
  unsigned int hashval;
  for (hashval=0;*s!=0;s++){
    hashval=*s+31*hashval;
  }
  return hashval % 1024;
}

// Check if the given p2p_id is in range r, inclusively
unsigned int isInRange(int n, Range * r){
  if(r->low > r->high){
    return (n >= r->low && n <= 1023) || (n <= r->high && n >= 0);
  } else {
    return (n >= r->low) && (n <= r->high);
  }
}

Player* constructPlayer(unsigned char *userdata, int offset){
  Player * player = (Player *) malloc (sizeof(Player));
  unsigned char name[10];
  int hp,exp,x,y;

  // Copy to intermediate data
  struct playerpacket * pp = (struct playerpacket*) (userdata+offset);
  strcpy(player->name,pp->name);
  player->hp = ntohl(pp->hp);
  player->exp = ntohl(pp->exp);
  player->x = pp->x;
  player->y = pp->y;
  player->p2p_id = calc_p2p_id(name);
  return player;
}

unsigned int handle_sendjoin(int sock,int myID){
  struct header *hdr=(struct header *) malloc(sizeof(int));
  struct p2p_join_request *jr=(struct p2p_join_request *) malloc(sizeof(struct p2p_join_request));

  hdr->version = 0x04;
  hdr->len = htons(0x8);
  hdr->msgtype = 0x10;

  jr->p2p_id = htonl(myID);
  unsigned char * payload_c = (unsigned char*) jr;
  unsigned char * header_c = (unsigned char*) hdr;
  unsigned char tosent[8];

  memcpy(tosent,header_c,4);
  memcpy(tosent+4,payload_c,4);

  int bytes_sent = send(sock,tosent,8,0);

  if( bytes_sent < 0 ){
    perror("send failed");
  }

  free(hdr);
  free(jr);
}

// Handling sending join response
unsigned int handle_sendjoinresponse(int sock,unsigned int count,unsigned char * userdata){
  struct header *hdr=(struct header *) malloc(sizeof(int));
  struct p2p_join_response *jr=(struct p2p_join_response *) malloc(sizeof(char)*(count*20+4));
  hdr->version = 0x04;
  hdr->len = htons(count*20+4+4); // userdata = count*20; usernumber = 4; header = 4
  hdr->msgtype = 0x11;

  jr->usernumber = htonl(count);

  unsigned char * payload_c = (unsigned char*) jr;
  unsigned char * header_c = (unsigned char*) hdr;
  unsigned char tosent[count*20+8];

  memcpy(tosent,header_c,4);
  memcpy(tosent+4,payload_c,4);
  memcpy(tosent+8,userdata,count*20);

  int bytes_sent = send(sock,tosent,count*20+8,0);

  if( bytes_sent < 0 ){
    perror("send failed");
  }
  
  free(hdr);
  free(jr);
}

int handle_sendbkuprequest(int sock, unsigned char * userdata){
  struct header *hdr=(struct header *) malloc(sizeof(int));
  hdr->version = 0x04;
  hdr->len = htons(0x8);
  hdr->msgtype = 0x13;

  unsigned char * header_c = (unsigned char*) hdr;
  unsigned char tosent[0x18];

  memcpy(tosent,header_c,4);
  memcpy(tosent+4,userdata,20);

  int bytes_sent = send(sock,tosent,0x18,0);

  if( bytes_sent < 0 ){
    perror("send failed");
  }
  
  free(hdr);
}

int handle_sendbkupresponse(int sock, unsigned char * userdata){
  struct header *hdr=(struct header *) malloc(sizeof(int));
  struct p2p_bkup_response *br=(struct p2p_bkup_response *) malloc(sizeof(char)*4);
  hdr->version = 0x04;
  hdr->len = htons(0x08);
  hdr->msgtype = 0x13;

  br->errorcode = 0x0;

  unsigned char * payload_c = (unsigned char*) br;
  unsigned char * header_c = (unsigned char*) hdr;
  unsigned char tosent[0x08];

  memcpy(tosent,header_c,4);
  memcpy(tosent+4,payload_c,4);

  int bytes_sent = send(sock,tosent,0x8,0);

  if( bytes_sent < 0 ){
    perror("send failed");
  }
  
  free(hdr);
  free(br);
}


// Find Predessesor and Succcess of the given p2p_id
serverInstance** findPredSucc(unsigned int p2p_id){
  serverInstance *predsucc[2];

  int succ_port,pred_port;
  char succ_ip[30];
  char pred_ip[30];

  char * name = "peers.lst";
  FILE * file = fopen(name,"re");

  if(file){
    // Initialize myport,p2p_id,L,S,LS,SL
    int pred,succ;
    int L = -1;
    int S = 1024;
    int SL = 1024;
    int LS = -1;
    int p2ptemp;
    char ipchartemp[20];
    int porttemp;
    int found = 0;

    // Move the cursor back
    rewind(file);

    while(fscanf(file,"%d%s%d",&p2ptemp,ipchartemp,&porttemp)!= EOF){
      if (p2ptemp != p2p_id){
	L = max(L,p2ptemp);
	S = min(S,p2ptemp);
	if (p2ptemp > p2p_id) SL = min(p2ptemp,SL);
	else LS = max(p2ptemp,LS);
      }
    }
    pred = LS; succ = SL;
    if(p2p_id > L) succ = S;
    if(p2p_id < S) pred = L;
    rewind(file);

    if (pred > -1 && succ < 1024){
      while(fscanf(file,"%d%s%d",&p2ptemp,ipchartemp,&porttemp)!= EOF){
	if(succ == p2ptemp){
	  succ_port = porttemp;
	  strcpy(succ_ip,ipchartemp);
	}
	if(pred == p2ptemp){
	  pred_port = porttemp;
	  strcpy(pred_ip,ipchartemp);
	}
      }

      serverInstance* pred_si = (serverInstance *) malloc(sizeof(serverInstance));
      serverInstance* succ_si = (serverInstance *) malloc(sizeof(serverInstance));

      pred_si->ip = (char *) malloc (sizeof(char)*30);
      succ_si->ip = (char *) malloc (sizeof(char)*30);
      
      // set pred/succ Server Instance Values
      pred_si->p2p_id = pred;
      pred_si->port = pred_port;
      strcpy(pred_si->ip,pred_ip);
      
      succ_si->p2p_id = succ;
      succ_si->port = succ_port;
      strcpy(succ_si->ip,succ_ip);
      
      // concatinate pred asd succ to create predsucc
      predsucc[0] = pred_si;
      predsucc[1] = succ_si;

    } else { // I am the only server
      predsucc[0] = NULL;
      predsucc[1] = NULL;
    }
    fclose(file);
    return predsucc;
  } else{ // Check for the file
    fprintf(stderr,"Error: %s.\n",strerror(errno));
    exit(0);
  }
}
