#include "../my_vm.h"
#include <time.h>
#include<math.h>
#include<pthread.h>

#define num_threads 15

#define MAX_SCORE 10.0

void *pointers[num_threads];
int ids[num_threads];
pthread_t threads[num_threads];
int alloc_size = 10000;
int matrix_size = 10;

void *alloc_mem(void *id_arg) {
    int id = *((int *)id_arg);
    pointers[id] = a_malloc(alloc_size);
    return NULL;
}

void *put_mem(void *id_arg) {
    int val = 1;
    void *va_pointer = pointers[*((int *)id_arg)];
    for (int i = 0; i < matrix_size; i++) {
        for (int j = 0; j < matrix_size; j++) {
            int address_a = (unsigned int)va_pointer + ((i * matrix_size * sizeof(int))) + (j * sizeof(int));
            put_value((void *)address_a, &val, sizeof(int));
			val++;
        }
    }
    return NULL;
}

void *mat_mem(void *id_arg) {
    int i = *((int *)id_arg);
    void *a = pointers[i];
    void *b = pointers[i + 1];
    void *c = pointers[i + 2];
    mat_mult(a, b, matrix_size, c);
    return NULL;
}

void *free_mem(void *id_arg) {
    int id = *((int *)id_arg);
    a_free(pointers[id], alloc_size);
}

int main() {
    srand(time(NULL));

    for (int i = 0; i < num_threads; i++)
        ids[i] = i;

    for (int i = 0; i < num_threads; i++)
        pthread_create(&threads[i], NULL, alloc_mem, (void *)&ids[i]);
    
    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);

    printf("Allocated Pointers: \n");
    for (int i = 0; i < num_threads; i++)
        printf("%x ", (int)pointers[i]);
    printf("\n");
    
    printf("initializing some of the memory by in multiple threads\n");
    for (int i = 0; i < num_threads; i++)
        pthread_create(&threads[i], NULL, put_mem, (void *)&ids[i]);

    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);

    printf("Randomly checking a thread allocation to see if everything worked correctly!\n");
    int rand_id = rand() % num_threads;
    void *a = pointers[rand_id];
    int val = 0;
    for (int i = 0; i < matrix_size; i++) {
        for (int j = 0; j < matrix_size; j++) {
            int address_a = (unsigned int)a + ((i * matrix_size * sizeof(int))) + (j * sizeof(int));
            get_value((void *)address_a, &val, sizeof(int));
            printf("%d ", val);
        }
        printf("\n");
    }

    printf("Performing matrix multiplications in multiple threads threads!\n");

    for (int i = 0; i < num_threads; i+=3)
        pthread_create(&threads[i], NULL, mat_mem, (void *)&ids[i]);

    for (int i = 0; i < num_threads; i+=3)
        pthread_join(threads[i], NULL);

    printf("Randomly checking a thread allocation to see if everything worked correctly!\n");
    rand_id = (((rand() % (num_threads/3)) + 1) * 3) - 1;
    a = pointers[rand_id];
    val = 0;

	int valid_mat[10][10];
	int check_mat[10][10];

    for (int i = 0; i < matrix_size; i++) {
        for (int j = 0; j < matrix_size; j++) {
            int address_a = (unsigned int)a + ((i * matrix_size * sizeof(int))) + (j * sizeof(int));
            get_value((void *)address_a, &val, sizeof(int));
			check_mat[i][j] = val;
            printf("%d ", val);
        }
        printf("\n");
    }
	
	int value = 1;
	for(int i = 0; i < matrix_size; i++){
		for(int j = 0; j < matrix_size; j++){
			valid_mat[i][j] = value++;
		}
	}

	printf("Verifying matrix\n");
	int valid = 0;
	int count = 0;
	for(int i = 0; i < matrix_size; i++){
		for(int j = 0; j < matrix_size; j++){
			for(int k = 0; k < matrix_size; k++){
				valid += valid_mat[i][k] * valid_mat[k][j];
			}
			if(valid != check_mat[i][j]) count++;
			printf("%d ", valid);
			valid = 0;
		}
		printf("\n");
	}
	
	int correct = matrix_size * matrix_size - count;
	printf("%d\n", correct);
	printf("Score: %d \n", (int) ceil(MAX_SCORE * ((double)correct)/((double)(matrix_size* matrix_size))));

}
