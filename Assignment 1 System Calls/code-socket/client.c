#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 5984
#define BUFF_SIZE 4096

int main(int argc, const char *argv[])
{
	int sock = 0;
	struct sockaddr_in serv_addr;
	char *hello = "Hello from client";
	char buffer[BUFF_SIZE] = {0};

	/* [C1]
	 * Here the socket function creates an endpoint for communication domain and returns a file descriptor that refers to that endpoint. 
	 * We requested for IPv4 protocol, TCP, Internet protocol (0) as parameters. If the return value is negative, it creates prints error and exists with error code.
	 */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return -1;
	}

	/* [C2]
	 * In the first line we are zeroing the datas structure setting everthing to 0 and then set the address family and the port. 
	 * As described in server.c , htons function ensure that it takes port in host byte order and converts it into network byte order. 
	 * The network protocol requires the transmitted packets to use network byte order.
	 */
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	/* [C3]
	 * Here we are converting the string "127.0.0.1" (localhost address where the server must be listening) 
	 * into a network address structure in the AF_INET address family, then copy the network address into 
	 * serv_addr data structure, storing it int sin_addr variable.
	 */
	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}

	/* [C4]
	 * Here the connect system call connects to the socket referred to by the file descriptor
	 *  sock to the address specified by serv_addr. In the third parameter we specify the length of the address. 
	 * The connect call gives a negative response, as a result of which the execution exits.
	 */
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("\nConnection Failed \n");
		return -1;
	}


	/* [C5]
	 * We are printing a message to user to press any key. 
	 * getchar() reads a character from std input. On a high level, this halts the execution of code until users gives an input.
	 */
	printf("Press any key to continue...\n");
	getchar();

	/* [C6]
	 * This initiates transmission of message from specified socket to peer.
	 * It sends messasge only when the socket is in connnected state. It uses the new_socket, the message(hello) here, length of message in bytes, 
	 * flags in type of message in transmission. Here no flags are specified.
	 */
	send(sock , hello , strlen(hello) , 0 );
	printf("Hello message sent\n");

	/* [C7]
	 * After sending data to server, it attems to read data from the socket and stores it into the buffer char array specified here. 
	 * The size of bytes of data specified here is 1024. On negative return code, means the operation failed and it exits. Finally it prints the data stored in buffer variable.
	 */
	if (read( sock , buffer, 1024) < 0) {
		printf("\nRead Failed \n");
		return -1;
    }
	printf("Message from a server: %s\n",buffer );
	return 0;
}
