#include <stdio.h>
#include <math.h>
#include "utils.h"

extern long long currBurstLength;

double test(double a, double b) {
 double c;
 c = 1.0 + sin (a + b*b);
 return c;
}

void Hello() {
  for (int i = 0; i < 5; i++)
  	test(0.0, 0.0);
  printf("Hello World %llu\n", currBurstLength);
}
