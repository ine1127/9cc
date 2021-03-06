DIR = $(shell pwd)

9cc: 9cc.c

test: 9cc
	./9cc -test
	./test.sh

.PHONY: test

clean:
	rm -f 9cc *.o *~ tmp* a.out

.PHONY: clean

a.out:
	gcc -g -O0 9cc.c

build:
	docker imabe build -t 9cc .

container:
	-docker container run --rm -it --cap-add=SYS_PTRACE --security-opt="seccomp=unconfined" -v $(DIR):/root 9cc
