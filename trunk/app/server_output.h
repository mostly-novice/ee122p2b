#ifndef SERVER_OUTPUT
#define SERVER_OUTPUT

void on_malformed_udp(int type){
  switch(type) {
  case 1:
    printf("Received malformed packet: wrong size.\n");
    break;
  case 2:
    printf("Received malformed packet: undefined type.\n");
    break;
  default:
    printf("Error: Undefined malformed type.\n");
    exit(0);
  }

  fflush(stdout);
}

void on_udp_duplicate(unsigned int ipaddr){
  printf("Received duplicate packet from: %x.\n",ipaddr);
  fflush(stdout);
}

#endif
