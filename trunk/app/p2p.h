unsigned int calc_p2p_id(unsigned char *s){
  unsigned int hashval;
  for (hashval=0;*s!=0;s++){
    printf("s:%x\n",*s);
    hashval=*s+31*hashval;
  }
  return hashval % 1024;
}

// Check if the given p2p_id is in range r, inclusively
unsigned int isInRange(int p2p_id, Range * r){
  return (p2p_id >= r->low) && (p2p_id <= r->high);
}

Player* constructPlayer(unsigned char *userdata){
  Player * player = (Player *) malloc (sizeof(Player));
  int offset = 0;
  unsigned char name[10];
  int hp,exp,x,y;

  // Copy to intermediate data
  memcpy(name,userdata+offset,10); offset+=10;
  memcpy(hp,userdata+offset,4);    offset+=4;
  memcpy(exp,userdata+offset,4);   offset+=4;
  memcpy(x,userdata+offset,1);     offset+=1;
  memcpy(y,userdata+offset,1);     offset+=1;

  // Initialize player
  strcpy(player->name,name);
  player->hp = ntohl(hp);
  player->exp = ntohl(exp);
  player->x = x;
  player->y = y;
  player->p2p_id = calc_p2p_id(name);

  return player;
}

unsigned int handle_sendjoin(int sock,int myID){
  struct header *hdr=(struct header *) malloc(sizeof(int));
  struct p2p_join_request *jr=(struct p2p_join_request *) malloc(sizeof(struct p2p_join_request));

  hdr->version = 0x04;
  hdr->len = htons(0x8);
  hdr->msgtype = htons(0x10);

  jr->p2p_id = htonl(myID);
  unsigned char * payload_c = (unsigned char*) jr;
  unsigned char * header_c = (unsigned char*) hdr;
  unsigned char * tosent = (unsigned char *) malloc(sizeof(hdr->len));

  printf("performing memcopies\n");
  memcpy(tosent,header_c,4);
  memcpy(tosent+4,payload_c,4);
  printf("DONE\n");

  int bytes_sent = send(sock,tosent,8,0);

  if( bytes_sent < 0 ){
    perror("send failed");
  }

  free(hdr);
  free(jr);
  free(tosent);
}

// Handling sending join response
unsigned int handle_sendjoinresponse(int sock,unsigned int count,unsigned char * userdata){
  struct header *hdr=(struct header *) malloc(sizeof(int));
  struct p2p_join_response *jr=(struct p2p_join_response *) malloc(sizeof(char)*(count*20+4));
  hdr->version = 0x04;
  hdr->len = htons(count*20+4+4); // userdata = count*20; usernumber = 4; header = 4
  hdr->msgtype = htons(0x11);

  jr->usernumber = htonl(count);

  unsigned char * payload_c = (unsigned char*) jr;
  unsigned char * header_c = (unsigned char*) hdr;
  unsigned char * tosent = (unsigned char *) malloc(sizeof(hdr->len));

  memcpy(tosent,header_c,4);
  memcpy(tosent+4,payload_c,4);
  memcpy(tosent+8,userdata,count*20);

  int bytes_sent = send(sock,tosent,hdr->len,0);

  if( bytes_sent < 0 ){
    perror("send failed");
  }

  free(hdr);
  free(jr);
  free(tosent);
}


// Find Predessesor and Succcess of the given p2p_id
serverInstance** findPredSucc(unsigned int p2p_id){
  serverInstance *predsucc[2];

  int succ_port,pred_port;
  char succ_ip[30];
  char pred_ip[30];
  FILE * file = fopen("peers.lst","r+");
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
	printf("p2p_temp:%d\n",p2ptemp);
	L = max(L,p2ptemp);
	S = min(S,p2ptemp);
	if (p2ptemp > p2p_id) SL = min(p2ptemp,SL);
	else LS = max(p2ptemp,LS);
      }
    }

    pred = LS;
    succ = SL;
    if(p2p_id > L)
      succ = S;
    if(p2p_id < S)
      pred = L;
    printf("L:%d\n",L);
    printf("S:%d\n",S);
    printf("LS:%d\n",LS);
    printf("SL:%d\n",SL);
    printf("Pred: %d\nSucc:%d\n",pred,succ);

    rewind(file);

    if (pred > -1 && succ < 1024){
      // Search through the database
      while(fscanf(file,"%d%s%d",&p2ptemp,ipchartemp,&porttemp)!= EOF){
	// FIND PORT/IP for pred and succ
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
      predsucc[0] = pred;
      predsucc[1] = succ;

    } else { // I am the only server
      predsucc[0] = NULL;
      predsucc[1] = NULL;
    }

    return predsucc;
	
  } else{ // Check for the file
    printf("THIS SHOULD NOT HAPPEN!!!\n");
    exit(0);
  }
}
