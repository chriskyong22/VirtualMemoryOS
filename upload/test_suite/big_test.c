#include<stdlib.h>
#include<stdio.h>
#include"../my_vm.h"
#include<unistd.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<math.h>

#define SIZE 163811

#define DIFF_MAX 20
#define AFREE 5

int main(){
	
	int total_score = 0;

	printf("Big a_malloc call\n");

	char* a_ptr = a_malloc(SIZE);
	
	char* text = malloc(SIZE);
	int fd = open("alice.txt", O_RDONLY);

	int read_bytes = 0;
	int total_bytes = 0;
	while((read_bytes = read(fd, text + total_bytes, SIZE - total_bytes)) != 0){
		total_bytes += read_bytes;
	}
	printf("Read in %d bytes from file with expected size %d\n", total_bytes, SIZE);
	close(fd);

	printf("Putting text into memory\n");
	put_value(a_ptr, text, SIZE);
	
	char* out = malloc(SIZE);
	printf("Getting text from memory\n");
	get_value(a_ptr, out, SIZE);

	fd = open("compare.txt", O_RDWR | O_CREAT, 0640);
	
	read_bytes = 0;
	int old_total = total_bytes;
	total_bytes = 0;
	while((read_bytes = write(fd, out + total_bytes, SIZE - total_bytes)) != 0){
		total_bytes += read_bytes;
	}
	if(SIZE != total_bytes) printf("Bytes written doesn't match\n");
	close(fd);

	int status = system("diff --text alice.txt compare.txt > /dev/null");
	switch(status){
		case 0: 
			total_score += DIFF_MAX;
			printf("No errors. Part 1 score: %d\n", total_score); break;
		default: 
			printf("Files don't match. Checking for differences. ");
			int bytes_checked = (total_bytes < old_total) ? total_bytes:old_total;
			int diff_count = 0;
			
			for(int i = 0; i < bytes_checked; i++){
				if(text[i] != out[i]) diff_count++;
			}
			//score is the percentage difference of character in the file (max 30)
			int unchecked = SIZE - bytes_checked;
			int correct = SIZE - (diff_count + unchecked);
			total_score += (int) ceil((double) DIFF_MAX * ((double)correct) / ((double)SIZE));
			
			if(total_score == DIFF_MAX){
				printf("Bytes in memory buffer match, but diff error given. Byte data probably corrupt.\n");
				total_score -= 1;
			}
			printf("Part 1 score: %d\n", total_score);
		//default: printf("Diff error. Part 1 score: 0\n"); break;
	}
	
	printf("Freeing data\n");
	unsigned long old = (unsigned long) a_ptr;
	a_free(a_ptr, SIZE);

	printf("Checking new allocations\n");
	//should only have remainder 1 page if any
	int pages = (SIZE % PGSIZE > 0) ? SIZE/PGSIZE + 1 : SIZE/PGSIZE;
	unsigned long ptrs[pages];
	for(int i = 0; i < pages; i++){
		ptrs[i] = (unsigned long) a_malloc(1);
	}
	if(ptrs[0] == old) printf("Starting page matches\n");
	else printf("Starting page doesn't match. Could have holes. Check basic_test results for holes.\n");
	
	unsigned long old_last = old + (pages-1)*PGSIZE;
	if(ptrs[pages-1] == old_last){
		printf("Last page matches\n");
		total_score += AFREE;
	}else printf("Last page doesn't match.\n");
	
	printf("Big test score: %d\n", total_score);
	return 0;
}
