#include<stdlib.h>
#include<stdio.h>
#include"../my_vm.h"

#define AMALLOC 5
#define PUT_GET 2
#define AFREE 5
#define DEDUCTIONS 2 //1 each for get and put
#define DEDUCTION_PTS 2 //2 pts deducted for each get/put

int main(int argc, char** argv){
	
	int total_score = 0;
	
	printf("Basic a_malloc call\n");
	int* a_ptr = a_malloc(sizeof(int)*5);
	
	if(a_ptr == 0x0) printf("Error: Address given as NULL!\n");
	else{
		printf("Basic a_malloc(): %d\n", AMALLOC);
		total_score += AMALLOC;
	}

	printf("Basic put_value calls\n");
	int x[5] = {1, 2, 3, 4, 5};
	put_value(a_ptr, x, sizeof(int)*5);

	printf("Basic get_value calls\n");
	int y[5];
	get_value(a_ptr, y, sizeof(int)*5);

	int pg_score = 0;
	printf("Checking array values\n");
	for(int i = 0; i < 5; i++){
		if(x[i] == y[i]){
			total_score += PUT_GET;
			pg_score += PUT_GET;
		}
	}
	printf("Basic Put and Get: %d\n", pg_score);

	printf("Basic a_free call\n");
	unsigned long old = (unsigned long) a_ptr;
	a_free(a_ptr, sizeof(int)*5);
	
	int deductions = 0;

	a_ptr = a_malloc(sizeof(int)*5);
	if(((unsigned long) a_ptr) != old){
		printf("a_free either didn't free or there is a hole\n");
		
		printf("Checking if free actually worked\n");
		int x = 0;
		get_value(((int*)old)+1, &x, sizeof(int));
		if(x == 2){
			printf("Error: get_value either doesn't check for invalid VA or free didn't work.\n");
			//if get/put score is already zero don't deduct - it's already a zero.
			if(pg_score > 0) deductions += DEDUCTIONS;
		}else{
			//don't give points if put/get didn't work - can't check if free worked with holes if put/get don't work
			if(pg_score > 0){
				printf("There is a hole, but free works: %d", AFREE);
				total_score += AFREE;
			}
		}
	}else{
		printf("Basic free: %d\n", AFREE);
		total_score += AFREE;
		
		printf("Freeing again to do invalid VA check\n");
		a_free(a_ptr, sizeof(int)*5);

		printf("Checking for get_value() invalid VA check\n");
		int x = 0;
		get_value(((int*) old)+1, &x, sizeof(int));
		if(x == 2){
			printf("Error: get_value doesn't check for invalid VA. Check put_value as well in submission code.\n");
			deductions += DEDUCTIONS;
		}
	}

	printf("Basic test score without deductions: %d\n", total_score);
	//need to check source code for this
	printf("Score with %d deductions: %d\n", deductions, total_score - deductions * DEDUCTION_PTS);

	return 0;
}
