
all : test
test: ../my_vm.h
	gcc test.c -L../ -lmy_vm -lm -m64 -o test
mtest: ../my_vm.h
	gcc multi_test.c -L../ -lmy_vm -lm -m64 -g -lpthread -o mtest
clean:
	rm -rf test mtest
