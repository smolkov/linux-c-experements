CFLAGS=-Wall -Wextra -O2 -lpthread -D_GNU_SOURCE -g
CC=gcc

all: thread_demo thread_pipe thread_domain process_domain_simple


thread_demo: thread_demo.c
	$(CC) $< -o $@ $(CFLAGS)

thread_pipe: thread_pipe.c
	$(CC) $< -o $@ $(CFLAGS)

thread_domain: thread_domain.c
	$(CC) $< -o $@ $(CFLAGS)

process_domain_simple: process_domain_simple.c
	$(CC) $< -o $@ $(CFLAGS)


clean:
	rm -f thread_demo thread_pipe thread_domain 
	
rebuild: clean all


