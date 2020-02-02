#include <stdio.h>
#include <math.h>
#include "utils.h"

//extern long long global_var_1;

extern long long global_var_3;

double test2(double a, double b) {
 double c;
 c = 1.0 + sin (a + b*b);
 return c;
}

void Hello2() {
  //for (int i = 0; i < 5; i++)
  //	test(0.0, 0.0);
  printf("Hello World %llu\n", global_var_3);
}
