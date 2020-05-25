triangle: triangle.cpp glad.o
	g++ triangle.cpp glad.o -o triangle -lglut -lGL -lGLU -lglfw3 -lGL -lX11 -lpthread -lXrandr -lXi -ldl

triangle2: triangle2.cpp glad.o
	g++ triangle2.cpp glad.o -o triangle2 -lglut -lGL -lGLU -lglfw3 -lGL -lX11 -lpthread -lXrandr -lXi -ldl

triangle3: triangle3.cpp glad.o
	g++ triangle3.cpp glad.o -o triangle3 -lglut -lGL -lGLU -lglfw3 -lGL -lX11 -lpthread -lXrandr -lXi -ldl

.PHONY : everything

everything: triangle triangle2 triangle3

all: everything

clean:
	rm -f triangle triangle2 triangle3
