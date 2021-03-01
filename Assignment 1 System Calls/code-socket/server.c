#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 5984
#define BUFF_SIZE 4096

int main(int argc, const char *argv[])
{
	int server_fd, new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[BUFF_SIZE] = {0};
	char *hello = "Hello from server";

	/* [S1]
	 * Here the socket fucntion creates an endpoint for communication and returns a file descriptor that refers to that endpoint. 
	 * We requested for IPv4 protocol, TCP, Internet protocol (0) as parameters. If the return value is negative, it creates prints error and exists with error code.
	 */
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	/* [S2]
	 * We are setting socket options to the socket referred by file descripter. 
	 * From the stackoverflow reference, SO_REUSEPORT allows you to bind an arbitrary number of sockets to 
	 * exactly the same source address and port as long as all prior bound sockets also had SO_REUSEPORT set before they were bound.
	 * Hence essentially it helps in reuse of address and port
	 */
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
		       &opt, sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	/* [S3]
	 * We are setting the parameters in the sockaddr_in.  We set the address family, (ipv4 in this case). We bind it to any address (INADDR_ANY parameter), specifying the port to listen to. 
	 * htons function ensure that it takes port in host byte order and converts it into network byte order. The network protocol requires the transmitted packets to use network byte order.
	 */
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );

	/* [S4]
	 * We bind the address parameters set in the sockaddr data structure to the socket file descripter. Binding socket particular port at the server end so that client knows on which port to contact to the server. It case of an error, it prints error and exits with failure code.
	 */
	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	/* [S5]
	 * We are marking the socket referred by server_fd as a passive socket so that going forward it can accept incoming connection requests. 
	 * 3 specifies the backlog as to how many pending connection in server_fd can grow to.
	 */
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	/* [S6]
	 * Accept function extracts the first connection in the queue of the pending connections. 
	 * It returns the non-negative file descriptor of the accepted socket upon success. In case of failure it returns -1 
	 */
	if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
				 (socklen_t*)&addrlen)) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}

	/* [S7]
	 * We are printing a message to user to press any key. getchar() reads a character from std input. 
	 * On a high level, this halts the execution of code until users gives an input.
	 */
	printf("Press any key to continue...\n");
	getchar();

	/* [S8]
	 * This reads data from the socket at stores it into the buffer char array specified here. 
	 * The size of bytes of data specified here is 1024. On negative return code, means the operation failed and it exits. It prints the message the message stored in buffer variable.
	 */
	if (read( new_socket , buffer, 1024) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	printf("Message from a client: %s\n",buffer );

	/* [S9]
	 * This initiates transmission of message from specified socket to peer. 
	 * It sends messasge only when the socket is in connnected state. It uses the new_socket, the message(hello) here, 
	 * length of message in bytes, flags in type of message in transmission. Here no flags are specified.
	 */
	send(new_socket , hello , strlen(hello) , 0 );
	printf("Hello message sent\n");
	return 0;
}
