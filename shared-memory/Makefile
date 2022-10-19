CFLAGS=-Wall -Wextra -O2 -lpthread -D_GNU_SOURCE -g
CC=gcc


all: shmem_client shmem_server


shmem_client: shmem_client.c
	$(CC) $< -o $@ $(CFLAGS)


shmem_server:shmem_server.c
	$(CC) $< -o $@ $(CFLAGS)


clean:
	rm -f shmem_client shmem_server
	
rebuild: clean all


