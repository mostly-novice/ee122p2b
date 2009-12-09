#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  char hostname[30];
  gethostname(hostname);
  printf("gethostname:%s\n",hostname);

    struct hostent *hp = gethostbyname(hostname);

    if (hp == NULL) {
       printf("gethostbyname() failed\n");
    } else {
       printf("%s = ", hp->h_name);
       unsigned int i=0;
       while ( hp -> h_addr_list[i] != NULL) {
	 printf("ip in int:%x\n", *(hp -> h_addr_list[i]));
          printf( "%s ", inet_ntoa( *( struct in_addr*)( hp -> h_addr_list[i])));
          i++;
       }
       printf("\n");
    }
} 
