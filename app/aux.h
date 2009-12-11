int isVisible(int x1, int y1, int x2, int y2){
  return (abs(x1 - x2)<=5 && abs(y1 - y2)<=5);
}

int isnameinmap(char*name,char**fdnamemap){
  int i;
  for(i = 0; i < 22; i++){
    if(fdnamemap[i]){
      if(strcmp(name,fdnamemap[i])==0){
	return 1;
      }
    }
  }
  return 0;
}

int writeToFile(Player* player){
    FILE *file = fopen(player->name,"w+");
    fprintf(file,"%d %d %d %d",player->hp,player->exp,player->x,player->y);
    fclose(file);
    return 0;
}

void printMap(char ** fdnamemap){
  printf("Printing Map\n");
  int i;
  for(i=2; i< 20; i++){
    if(fdnamemap[i]){
      printf("sock:%d - name:%s\n",i,fdnamemap[i]);
    } else {
      printf("sock:%d - name:NULL\n",i);
    }
  }
}

char* getName(int fd,char*map[]){
  return map[fd];
}

void putName(int fd, char * name, char*map[]){
  char * buffer = (char*) malloc(sizeof(char)*strlen(name));
  strcpy(buffer,name);
  map[fd] = buffer;
}


int readstdin(char * command, char * arg){
  char mystring[300];
  char * pch;

  setvbuf(stdin,NULL,_IONBF,1024 );
  fgets(mystring,300,stdin);
  if(strcmp(mystring,"\n")==0){
    return 0;
  }
  mystring[strlen(mystring)-1] = 0;
  pch = strtok(mystring," ");
  strcpy(command,pch);

  pch += strlen(pch);

  while(*(pch)==' ' || *(pch)=='\0'){
    pch++;
  }

  strcpy(arg, pch);
  return 1;
}

void newconnection(char * ip, int port, int * sockargs){
  printf("Trying to connect to %s:%d\n",ip,port);
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0){ perror("socket() faild"); abort(); }

  *sockargs = sock;

  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));

  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = inet_addr(ip);
  sin.sin_port = htons(port);

  if(connect(sock,(struct sockaddr *) &sin, sizeof(sin)) < 0){
    //TODO: Handle Disconnection of predecessor
    perror("client - connect");
    close(sock);
    abort();
  }
}

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
