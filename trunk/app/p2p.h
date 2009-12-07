unsigned int calc_p2p_id(unsigned char *s){
  unsigned int hashval;
  for (hashval=0;*s!=0;s++){
    hashval=*s+31*hashval;
  }
  return hashval % 1024;
}
