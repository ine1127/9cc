9cc: 9cc.c

test: 9cc
	./test.sh

.PHONY: test

clean:
	rm -f 9cc *.o *~ tmp*

.PHONY: clean

