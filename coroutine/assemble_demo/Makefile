main: main.o save_restore.o
	gcc -o main main.o save_restore.o
main.o: main.c
	gcc -c -o main.o main.c
test_save_restore: save_restore.o test_save_restore.o
	gcc -o test_save_restore save_restore.o test_save_restore.o
save_restore.o: save_restore.S
	as --64 -o save_restore.o save_restore.S
test_save_restore.o: test_save_restore.c
	gcc -c -o test_save_restore.o test_save_restore.c

clean:
	rm -f main test_save_restore *.o
