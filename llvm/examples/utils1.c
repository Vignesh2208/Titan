#include <stdio.h>
#include <math.h>
#include "utils.h"

//extern long long global_var_1;
extern long long global_var_3;
extern int enabled_2;

double test(double a, double b) {
 double c;
 c = 1.0 + sin (a + b*b);
 return c;
}

void Hello() {
  for (int i = 0; i < 5; i++)
  	test(0.0, 0.0);

  global_var_3 = 100;
  global_var_3 = global_var_3 - 5;

  if (enabled_2 || global_var_3 < 0) 
  	printf("Hello World %llu\n", global_var_3);
}
