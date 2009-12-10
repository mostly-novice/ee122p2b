int main() {
  int a = 0xffffffc0;
  char * achar = (char*)&a;
  printf("a1:%x",achar[0]);
  char array[2];
  int i;
  memcpy(array,achar,4);
  printf("array:%x%x\n",array[0],array[1],array[2],array[3]);
  return 0;
}
