#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

int isInRange(int low, int high, int n){
  if(low>high){
    return ((n>=low && n <= 1023) || (n<= high && n >= 0)); 
  } else {
    return (n>=low && n <= high);
  }
}

int main (void)
{
  printf("Should be true: %d\n", isInRange(80,0,200));
  printf("Should be true: %d\n", isInRange(1023,10,2));
  printf("Should be true: %d\n", isInRange(0,100,20));
  printf("Should be false: %d\n", isInRange(10,110,200));
  printf("Should be false: %d\n", isInRange(80,10,79));
  printf("Should be false: %d\n", isInRange(80,10,11));
  printf("Should be false: %d\n", isInRange(10,110,2));
  
   return 0;
}
