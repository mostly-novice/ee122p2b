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

void newconnection(char * ip, int port, int * sockargs){
  printf("Connecting to %s:%d\n",ip,port);
  int done = 0;
  while(!done){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){ perror("socket() faild"); abort(); }
    
    *sockargs = sock;
    
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(ip);
    sin.sin_port = htons(port);
    
    int val = connect(sock,(struct sockaddr *) &sin, sizeof(sin));
    
    if( val >= 0){
      printf("Successfully connected.\n");
      done = 1;
    } else {
      perror("newconection - Fail to connect");
      printf("Retrying...\n");
      close(sock);
    }
  }
}
