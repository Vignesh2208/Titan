#include <stdio.h>

long long currBurstLength = 1000;
long long currBBID = 0;
long long currBBSize = 0;
int alwaysOn = 1;
int counter = 0;

void vtCallbackFn() {
  counter ++;
  printf("vtCallback: currBurstLength = %llu, currBBID = %llu, currBBSize = %llu, counter = %d\n", currBurstLength, currBBID, currBBSize, counter);
}
