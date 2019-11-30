.PHONY: everything

everything: hello hello_another

hello: hello.c
	gcc -g -o $@ $<

hello_another: hello_another.c
	gcc -g -o $@ $<

clean:
	rm -rf hello hello_another
