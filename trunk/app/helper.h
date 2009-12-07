#include <stdio.h>
#include <stdlib.h>
#include "constants.h"

void printMessage(char * message, int len){
  int i;
  for(i = 0; i < len; i++){
    printf("%02x ", *(message+i));
  }
  printf("\n");
}

//Helper: handle the various task in clients
// TESTED
int handlelogin(char* name,int sock){
  int i = 0;
  int j;
  // Header
  struct header *hdr = (struct header *) malloc(sizeof(int)); // remember to free this
  struct login_request * payload = (struct login_request *) malloc(sizeof(int)*3); // remember to free this
  hdr->version = 0x04;
  hdr->len = htons(0x0010);
  hdr->msgtype = 0x1;
  strcpy(payload->name,name);
  unsigned char * payload_c = (unsigned char*) payload;
  unsigned char * header_c = (unsigned char *) hdr;
  unsigned char tosent[16];

  for(j = 0; j < 16; j++){
    if(j<4) tosent[j] = header_c[j];
    else tosent[j] = payload_c[j-4];
  }

  // Send a login message to the server
  int bytes_sent = send(sock, tosent,16,0);
  if (bytes_sent < 0) perror("send failed");
  free(hdr);
  free(payload);
  return 0;
}

// TESTED
int handlemove(unsigned char d, int sock){
  struct header *hdr = (struct header *) malloc(sizeof(int)); // Remember to free this
  struct move * payload = (struct move *) malloc(sizeof(int)); // Remember to free this
  int j;

  hdr->version = 0x4;
  hdr->len = htons(0x08);
  hdr->msgtype = MOVE;

  payload->direction = d;

  unsigned char * payload_c = (unsigned char*) payload;
  unsigned char * header_c = (unsigned char *) hdr;
  unsigned char tosent[8];

  for(j = 0; j < 8; j++){
    if(j<4){
      tosent[j] = header_c[j];
    } else {
      tosent[j] = payload_c[j-4];
    }
  }
  
  int bytes_sent = send(sock,tosent,8,0);
  if (bytes_sent < 0){
    perror("send failed.\n");
  }

  // Freeing the pointers;
  free(hdr);
  free(payload);

  return 0;
}


// Helper for attack
// Assuming that the input is valid
// @TODO: Test
int handleattack(char * victim, int sock){
  int j = 0;

  // Sending message
  struct header *hdr = (struct header *) malloc(sizeof(int)); // remember to free this
  struct attack * payload = (struct attack *) malloc(sizeof(int)*3); // remember to free this

  hdr->version = 0x4;
  hdr->len = htons(0x10);
  hdr->msgtype = ATTACK;

  // Copy the name
  strcpy(payload->victimname,victim);

  unsigned char * payload_c = (unsigned char*) payload;
  unsigned char * header_c = (unsigned char *) hdr;

  unsigned char tosent[16];

  for(j = 0; j < 16; j++){
    if(j<4){
      tosent[j] = header_c[j];
    } else {
      tosent[j] = payload_c[j-4];
    }
  }

  // Sending
  int bytes_sent = send(sock,tosent,16,0);
  if (bytes_sent < 0){
    perror("send failed.\n");
    return -1;
  } else {
    //printf("Send: %d bytes\n", bytes_sent);
  }

  free(hdr);
  free(payload);
  return 0;
}

// Speak
int handlespeak(char * m, int sock){
 // Header

  unsigned int payloadLength = strlen(m) + 1 + 4 - ((strlen(m)+1) % 4);
  struct header *hdr = (struct header *) malloc(sizeof(int)); // remember to free this

  //msg = (char *) malloc(sizeof(char)*(strlen(m)+1));

  int j;

  unsigned int totalMessageLength = payloadLength + 4;

  hdr->version = 0x4;
  hdr->len = htons(totalMessageLength);
  hdr->msgtype = SPEAK;

  unsigned char * header_c = (unsigned char *) hdr;

  unsigned char tosent[totalMessageLength];

  for(j = 0; j < totalMessageLength; j++){
    if(j<4){
      tosent[j] = header_c[j];
    } else {
      tosent[j] = *(m+j-4);
    }
  }

  // Send a speak message to the server
  int bytes_sent = send(sock, tosent,totalMessageLength,0);
  if (bytes_sent < 0) {
    perror("send failed");
  } else {
    //printf("Sent: %d bytes\n", bytes_sent);
  }

  free(hdr);
  return 0;
}

int handlelogout(char * name,int sock){
  struct header *hdr = (struct header *) malloc(sizeof(int));
  int j;

  hdr->version = 0x4;
  hdr->len = htons(0x4);
  hdr->msgtype = LOGOUT;

  char * header_c = (char*) hdr;
  unsigned char tosent[4];

  for(j = 0; j < 4; j++){
      tosent[j] = header_c[j];
  }

  // Sending the message;
  int bytes_sent = send(sock,tosent,4,0);
  if (bytes_sent < 0){
    perror("send failed.\n");
    free(hdr);
  }
  free(hdr);
}

