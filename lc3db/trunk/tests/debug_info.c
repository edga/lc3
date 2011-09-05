#include <stdio.h>

typedef struct {
 int a;
 char b;
} Tstruct, *PTstruct;


Tstruct glob_struct = {1,2};
Tstruct regs = {1,2};

int glob_foo[1000];


static functionStatic(int sarg_a) {

	return sarg_a;
}

int functionABC(int arg_a, char arg_b, char * arg_c) {
	int loc_ABC_1;
	register int loc_reg_ABC_2;

	loc_ABC_1 = arg_a+functionStatic(arg_b);
	loc_reg_ABC_2 = arg_a+arg_c[0];

	return loc_ABC_1 * loc_reg_ABC_2;
}



int main()
{
   static int sloc_x;
   int loc_limit;
   int loc_sum = 0;
   int common = 0;
   PTstruct loc_pstruct= &glob_struct;

   {
      int common = 2;
      sloc_x = functionABC(1, 'a', "Hello");
   }

   printf("Enter number:");
   scanf("%d", &loc_limit);

   for (sloc_x = 0; sloc_x < loc_limit; sloc_x++)
   {
      loc_sum = loc_sum + sloc_x;
      printf("sum[%d] == %d\n", sloc_x, loc_sum);
   }
   printf("Done\n");

   return 0;
}
