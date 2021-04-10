all : test mtest
test: ../my_vm.h
	gcc -m32 test.c -L../ -lmy_vm -o test
mtest: ../my_vm.h
	gcc -m32 multi_test.c -L../ -lmy_vm -pthread -o mtest
test64: ../my_vm.h
	gcc -m64 test.c -L../ -lmy_vm64 -o test64
mtest64: ../my_vm.h
	gcc -m64 multi_test.c -L../ -lmy_vm64 -pthread -o mtest64
clean:
	rm -rf test mtest test64 mtest64
