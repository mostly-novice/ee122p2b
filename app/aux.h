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

// Printing out the relevant statistics
void printStat(){
  printf("\n");
  printf("Number of mallocs:%d\n",mc);
  printf("Number of frees:%d\n",fc);
  printf("\n");
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
