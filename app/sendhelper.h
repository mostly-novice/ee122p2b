// Helper to send messages.
unsigned char * createloginreply( unsigned char error_code,
				  unsigned int hp,
				  unsigned int exp,
				  unsigned char x,
				  unsigned char y,
				  char buffer[]
				  ){
  int i = 0;
  int j;
  struct header *hdr = (struct header *) malloc(sizeof(int));
  struct login_reply * payload = (struct login_reply *) malloc(sizeof(int)*3);

  hdr->version = 0x04;
  hdr->len = htons(0x0010);
  hdr->msgtype = LOGIN_REPLY;

  payload->error_code = error_code;
  payload->hp = htonl(hp);
  payload->exp = htonl(exp);
  payload->x = x;
  payload->y = y;
  unsigned char * payload_c = (unsigned char*) payload;
  unsigned char * header_c = (unsigned char *) hdr;
  for(j = 0; j < 16; j++){
    if(j<4) buffer[j] = header_c[j];
    else buffer[j] = payload_c[j-4];
  }
  free(hdr);
  free(payload);
}

void createmovenotify(char* name,
		      unsigned int hp,
		      unsigned int exp,
		      unsigned char x,
		      unsigned char y,
		      char buffer[]){
  int i = 0;
  int j;
  // Header
  struct header *hdr = (struct header *) malloc(sizeof(int)); // remember to free this
  struct move_notify * payload = (struct move_notify *) malloc(sizeof(int)*5); // remember to free this
  hdr->version = 0x04;
  hdr->len = htons(0x0018);
  hdr->msgtype = MOVE_NOTIFY;
  for(i = 0; i < 10; i++){
    payload->name[i] = *(name+i);
  }
  payload->hp = htonl(hp);
  payload->exp = htonl(exp);
  payload->x = x;
  payload->y = y;

  unsigned char * payload_c = (unsigned char*) payload;
  unsigned char * header_c = (unsigned char *) hdr;	// flatten header struct
  for(j = 0; j < 24; j++){
    if(j<4) buffer[j] = header_c[j];
    else buffer[j] = payload_c[j-4];
  }
  free(hdr);
  free(payload);
}

unsigned char * createattacknotify(unsigned char* attacker_name,
				   unsigned char* victim_name,
				   unsigned int hp,
				   unsigned char damage,
				   char buffer[]){
  int i = 0;
  int j;
  // Header
  struct header *hdr = (struct header *) malloc(sizeof(int)); // remember to free this
  struct attack_notify * payload = (struct attack_notify *) malloc(sizeof(int)*7); // remember to free this
  hdr->version = 0x04;
  hdr->len = htons(0x0020);
  hdr->msgtype = ATTACK_NOTIFY;

  payload->hp = htonl(hp);
  payload->damage = damage;
  memcpy(payload->attacker_name,attacker_name,10);
  memcpy(payload->victim_name,victim_name,10);

  unsigned char * payload_c = (unsigned char*) payload;
  unsigned char * header_c = (unsigned char *) hdr;

  for(j = 0; j < 32; j++){
    if(j<4) buffer[j] = header_c[j];
    else buffer[j] = payload_c[j-4];
  }

  // Send a login message to the server
  free(hdr);
  free(payload);
}


unsigned char * createspeaknotify(unsigned char* broadcaster,
				  char* message,
				  int totallen,
				  char buffer[]){
  int i = 0;
  int j;
  struct header *hdr = (struct header *) malloc(sizeof(int)); // remember to free this
  hdr->version = 0x04;
  hdr->len = htons(totallen);
  hdr->msgtype = SPEAK_NOTIFY;
  unsigned char * header_c = (unsigned char *) hdr;
  for(j = 0; j < totallen; j++){
    if(j<4){
      buffer[j] = header_c[j];
    } else if (j<14){
      buffer[j] = *(broadcaster+j-4);
    } else {
      buffer[j] = *(message+(j-14));
    }
  }
  free(hdr);
}

unsigned char * createlogoutnotify(Player* player,char buffer[]){
  int i = 0;
  int j;
  // Header
  struct header *hdr = (struct header *) malloc(sizeof(int)); // remember to free this
  struct logout_notify * payload = (struct logout_notify *) malloc(sizeof(int)*5); // remember to free this
  hdr->version = 0x04;
  hdr->len = htons(0x00018);
  hdr->msgtype = LOGOUT_NOTIFY;

  strcpy(payload->name,player->name);

  payload->hp = htonl(player->hp);
  payload->exp = htonl(player->exp);
  payload->x = player->x;
  payload->y = player->y;

  unsigned char * payload_c = (unsigned char*) payload;
  unsigned char * header_c = (unsigned char *) hdr;

  for(j = 0; j < 24; j++){
    if(j<4) buffer[j] = header_c[j];
    else buffer[j] = payload_c[j-4];
  }

  free(hdr);
  free(payload);
}

void createinvalidstate(unsigned char error_code, char buffer[]){
  int i = 0;
  int j;
  // Header
  struct header *hdr = (struct header *) malloc(sizeof(int)); // remember to free this
  struct invalid_state * payload = (struct invalid_state *) malloc(sizeof(int)); // remember to free this
  hdr->version = 0x04;
  hdr->len = htons(0x0008);
  hdr->msgtype = INVALID_STATE;

  payload->error_code = error_code;

  unsigned char * payload_c = (unsigned char*) payload;
  unsigned char * header_c = (unsigned char *) hdr;

  for(j = 0; j < 8; j++){
    if(j<4) buffer[j] = header_c[j];
    else buffer[j] = payload_c[j-4];
  }

  free(hdr);
  free(payload);
}

void createpsr(char *name, unsigned int hp, unsigned int exp, unsigned char x, unsigned char y, unsigned int id,char buffer[]){
  struct player_state_response * psr = (struct player_state_response *) malloc(sizeof(struct player_state_response));

  strcpy(psr->name,name);
  psr->message_type = PLAYER_STATE_RESPONSE;
  psr->id   = id;
  psr->hp   = htonl(hp);
  psr->exp  = htonl(exp);
  psr->x    = x;
  psr->y    = y;

  char * toreturn = (char*) psr;
  memcpy(buffer,toreturn,PLAYER_STATE_RESPONSE_SIZE);
}

void create_ss_response(int id,char success,char buffer[]){
    struct save_state_response * ssr = (struct save_state_response *) malloc (sizeof(struct save_state_response));

    ssr->message_type = SAVE_STATE_RESPONSE;
    ssr->id = id;
    ssr->error_code = success;
    char * toreturn = (char*) ssr;
    int j;

    // Copy the 8 bytes by hand -- to ensure no mistake is made
    for(j = 0; j < 8; j++){
        buffer[j] = toreturn[j];
    }
}

int broadcast(fd_set login, int sock, int fdmax, unsigned char * tosent,int expected){
  int i;
  for(i=0; i<fdmax+1;i++){
    if (FD_ISSET(i,&login)){
      int bytes_sent = send(i, tosent,expected,0);
      if (bytes_sent < 0) perror("send failed");
    }
  }
}

/*
 * sock is the socket you want to send to
 * tosent is the packet you want to send
 * expected: is the size you expected
 */
int unicast(int sock, unsigned char * tosent, int expected){
  int bytes_sent = send(sock, tosent,expected,0);
  if (bytes_sent < 0){
    perror("send failed");
    abort();
  }
  return 1;
}

int udpunicast(int udpsock, struct sockaddr_in targetsin, unsigned char * tosent, int expected){
  int bytes_sent = sendto(udpsock, tosent,expected,0,(struct sockaddr *) &targetsin,sizeof(targetsin));
  if (bytes_sent < 0){
    perror("send failed");
    abort();
  }
  return 1;
}
