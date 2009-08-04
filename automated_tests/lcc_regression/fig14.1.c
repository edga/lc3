#include <stdio.h>

void PrintBanner();     /* Function declaration */

int main()
{
   PrintBanner();       /* Function call        */
   printf("A simple C program.\n");
   PrintBanner();
   return 0;
}

void PrintBanner()      /* Function definition  */
{
   printf("============================\n");
}

