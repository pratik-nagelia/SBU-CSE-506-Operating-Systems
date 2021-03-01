#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdio.h>
#include <linux/userfaultfd.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <poll.h>
#define BUFF_SIZE 4096

static int page_size;

int main(int argc, char *argv[])
{
	int local_server_fd, remote_socket, new_socket;
	struct sockaddr_in local_addr, remote_addr;
	int opt = 1, first_connection_failed = 0 ;
	int addrlen = sizeof(local_addr);
	char buffer[BUFF_SIZE] = {0};
	char *addr;

	unsigned long local_port, remote_port, num_pages = 0 , len = 0;

	/* Handling Input sequences */
	if (argc != 3) {
		fprintf(stderr, "Enter two arguments. Usage: %s local_port remote_port \n", argv[0]);
		exit(EXIT_FAILURE);
	}
	local_port =  strtoul(argv[1], NULL, 0);
	remote_port =  strtoul(argv[2], NULL, 0);
	printf("Local Port entered : %lu \n", local_port);
	printf("Remote Port entered : %lu \n", remote_port);


	/* Socket Communication */
	if ((local_server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Local Socket creation failed \n");
		exit(EXIT_FAILURE);
	}

	if ((remote_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Remote creation error \n");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(local_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	local_addr.sin_port = htons(local_port);

	memset(&remote_addr, '0', sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(remote_port);

	if (bind(local_server_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
		perror("Binding failed");
		exit(EXIT_FAILURE);
	}

	if(inet_pton(AF_INET, "127.0.0.1", &remote_addr.sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}

	if (listen(local_server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	printf("Started Listening on port %lu ...\n", local_port);


	/* Starting pairing requests */
	if (!first_connection_failed){
		if (connect(remote_socket, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
			printf("Initial Connection Attempt Failed \n");
			first_connection_failed++;
		} else {
			printf("Initial Connection Attempt Succeeded \n");
		}
	}
	
	printf("Waiting to accept connetion from remote \n");
	if ((new_socket = accept(local_server_fd, (struct sockaddr *)&local_addr,
				 (socklen_t*)&addrlen)) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	printf("Accepted Connection from remote port \n");

	if (first_connection_failed) {
		if (connect(remote_socket, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
			printf("Subsequent Connection Attempt Failed \n");
		} else {
			printf("Subsequent Connection Attempt Succeeded \n");
		}
	}
	
	printf("first connection_failed %d \n", first_connection_failed);

	/* If first_connection_failed, then this is the first process */
	if (first_connection_failed)
	{
		printf("How many pages would you like to allocate (greater than 0)? \n");
		if(scanf("%lu", &num_pages) == EOF ) { perror("EOF on scanning input");}
		page_size = sysconf(_SC_PAGE_SIZE);
		len = num_pages * page_size;	
		addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		printf("Address mapping created : %p\n", addr);
        snprintf(buffer, BUFF_SIZE, "%lu-%p", len, addr);
		send(remote_socket, buffer , strlen(buffer) , 0 );


	} else { 
		if (read(new_socket , buffer, 1024) < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}
		char *token;
		token = strtok(buffer, "-"); 
		len = strtoul(token, NULL, 0);
		printf("length received from peer %lu\n", len);
		token = strtok(NULL, "-");
		addr = token;
		printf("Address received from peer: %s \n", token);
		sscanf(token, "%p", &addr);
		addr = mmap(addr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		printf("Address returned by mmap(): %p \n", addr);

		if (addr == MAP_FAILED) {
			perror("error in address creation in mmap ");
			exit(EXIT_FAILURE);
		}

	}

	return 0;
}
