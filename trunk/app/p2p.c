#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  int p2p_id = atoi(argv[1]);
  char * ipchar = argv[2];
  int port = atoi(argv[3]);

  FILE * file = fopen("peers.lst","r+");
  if(file){
    int p2ptemp;
    char ipchartemp[30];
    int porttemp;
    rewind(file);
    int found = 0;
    while(fscanf(file,"%d%s%d",&p2ptemp,ipchartemp,&porttemp)!= EOF){
      printf("p2ptemp:%d\n",p2ptemp);
      if (p2ptemp == p2p_id){
	if (strcmp(ipchartemp,ipchar)!=0 || porttemp != port){
	  printf("Conflicts\n");
	  exit(0);
	} else {
	  found = 1;
	  printf("Sending JOIN message\n");
	  break;
	}
      }
    }
    if(!found){
      fseek(file,0,SEEK_END);
      fprintf(file,"%d %s %d\n", p2p_id,ipchar,port);
    }
  } else {
    file = fopen("peers.lst","w+");
    fprintf(file,"%d %s %d\n", p2p_id,ipchar,port);
  }
  fclose(file);
}
