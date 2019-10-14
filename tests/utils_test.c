#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int convertStrtoArr(char * str, int * arr, int arr_size) 
{ 
    // get length of string str 
    int str_length = strlen(str); 
    int i = 0, j = 0;
    memset(arr, 0, sizeof(int)*arr_size);

  
    // Traverse the string 
    for (i = 0; str[i] != '\0'; i++) { 
  
        // if str[i] is ',' then split 
        if (str[i] == ',') { 
  
            // Increment j to point to next 
            // array location 
            j++; 
        } 
        else if (j < arr_size) { 
  
            // subtract str[i] by 48 to convert it to int 
            // Generate number by multiplying 10 and adding 
            // (int)(str[i]) 
            arr[j] = arr[j] * 10 + (str[i] - 48); 
        } 
    } 
  
   
    for (i = 0; i <= j; i++) { 
        printf("arr[%d] = %d\n", i, arr[i]);
    } 
  
    return j + 1;
} 
  
// Driver code 
int main() 
{ 
    char * str = "2,6,3,14"; 
    int arr[10], n_extracted;
    n_extracted = convertStrtoArr(str, arr, 10); 
    printf ("N_extracted = %d\n", n_extracted);
  
    return 0; 
} 