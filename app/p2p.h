unsigned int calc_p2p_id(unsigned char *s){
  unsigned int hashval;
  for (hashval=0;*s!=0;s++){
    hashval=*s+31*hashval;
  }
  return hashval % 1024;
}

unsigned int handle_sendjoin(int sock,int myID){
	struct header *hdr=(struct header *) malloc(sizeof(int));
	struct p2p_join_request *jr=(struct p2p_join_request *) malloc(sizeof(struct p2p_join_request));

	hdr->version = 0x04;
	hdr->len = htons(0x8);
	hdr->msgtype = htons(0x10);

	jr->P2P_ID = htonl(myID);
	unsigned char * payload_c = (unsigned char*) jr;
	unsigned char * header_c = (unsigned char*) hdr;
	unsigned char * tosent = (unsigned char *) malloc(sizeof(hdr->len));

	memcpy(tosent,header_c,4);
	memcpy(tosent+4,payload_c,4);

	int bytes_sent = send(sock,tosent,8,0);

	if( bytes_sent < 0 ){
		perror("send failed");
	}

	free(hdr);
	free(jr);
	free(tosent);
}
