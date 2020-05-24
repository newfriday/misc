version: version.c
	gcc version.c -o version -lglut -lGL -lGLU

hello: hello.cpp glad.o
	g++ hello.cpp glad.o -o hello -lglut -lGL -lGLU -lglfw3 -lGL -lX11 -lpthread -lXrandr -lXi -ldl

glad.o: glad.c
	gcc -c glad.c -o glad.o

clean:
	rm -f hello version
