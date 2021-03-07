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
#include <pthread.h>

#define BUFF_SIZE 4096

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE);    \
    } while (0)

static inline void ignore_return() {}

static void *fault_handler_thread(void *arg);

static int page_size;

void printPageContents(const char *addr, unsigned long num_pages, int request_page) {
    for (int i = ((request_page == -1) ? 0 : request_page);
         i <= ((request_page == -1) ? (num_pages - 1) : request_page); ++i) {
        printf(" [*] Page %d: \n", i);
        printf("%s\n", addr + (i * page_size));
    }
}

void updatePageContents(char *addr, int request_page, unsigned long num_pages, const char *message) {
    int l;
    for (int i = ((request_page == -1) ? 0 : request_page);
         i <= ((request_page == -1) ? num_pages - 1 : request_page); ++i) {
        l = 0x0 + (i * page_size);
        for (int j = 0; message[j] != '\0'; ++j) {
            addr[l++] = message[j];
        }
        addr[l] = '\0';
    }
}

int main(int argc, char *argv[]) {
    int local_server_fd, remote_socket, new_socket;
    struct sockaddr_in local_addr, remote_addr;
    int opt = 1, first_connection_failed = 0;
    int addrlen = sizeof(local_addr);
    char buffer[BUFF_SIZE] = {0};
    char *addr;

    unsigned long local_port, remote_port, num_pages = 0, len = 0;

    /* Handling Input ports */
    if (argc != 3) {
        fprintf(stderr, "Enter two arguments. Usage: %s local_port remote_port \n", argv[0]);
        exit(EXIT_FAILURE);
    }
    local_port = strtoul(argv[1], NULL, 0);
    remote_port = strtoul(argv[2], NULL, 0);
    printf("Local Port entered : %lu \n", local_port);
    printf("Remote Port entered : %lu \n", remote_port);


    /* Socket creation and logistics for setting up communication for pairing local and remote */
    if ((local_server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        errExit("Local Socket creation failed \n");
    }

    if ((remote_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        errExit("Remote socket creation failed \n");
    }

    if (setsockopt(local_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        errExit("setsockopt");
    }

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(local_port);

    memset(&remote_addr, '0', sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(remote_port);

    if (bind(local_server_fd, (struct sockaddr *) &local_addr, sizeof(local_addr)) < 0) {
        errExit("Binding failed");
    }

    if (inet_pton(AF_INET, "127.0.0.1", &remote_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (listen(local_server_fd, 3) < 0) {
        errExit("listen");
    }
    printf("Started Listening on port %lu ...\n", local_port);


    /* Starting pairing of two processes */
    if (!first_connection_failed) {
        if (connect(remote_socket, (struct sockaddr *) &remote_addr, sizeof(remote_addr)) < 0) {
            printf("Initial Connection Attempt Failed \n");
            first_connection_failed++;
        } else {
            printf("Initial Connection Attempt Succeeded \n");
        }
    }

    printf("Waiting to accept connection from remote \n");
    if ((new_socket = accept(local_server_fd, (struct sockaddr *) &local_addr,
                             (socklen_t *) &addrlen)) < 0) {
        errExit("accept");
    }
    printf("Accepted Connection from remote instance \n");

    if (first_connection_failed) {
        if (connect(remote_socket, (struct sockaddr *) &remote_addr, sizeof(remote_addr)) < 0) {
            printf("Subsequent Connection Attempt Failed \n");
        } else {
            printf("Subsequent Connection Attempt Succeeded \n");
        }
    }

    /* If first_connection_failed, then this is the first process and it would ask for user input */
    if (first_connection_failed) {
        printf("How many pages would you like to allocate (greater than 0)? \n");
        if (scanf("%lu", &num_pages) == EOF) { perror("EOF on scanning input"); }
        page_size = sysconf(_SC_PAGE_SIZE);
        len = num_pages * page_size;
        addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (addr == MAP_FAILED) {
            perror("Error in address allocation in mmap ");
            exit(EXIT_FAILURE);
        }
        printf("The mmapped memory size : %lu\n", len);
        printf("Address mapping created at : %p\n", addr);
        snprintf(buffer, BUFF_SIZE, "%lu-%p", len, addr);
        send(remote_socket, buffer, strlen(buffer), 0);

    } else {
        if (read(new_socket, buffer, 1024) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        char *token;
        token = strtok(buffer, "-");
        len = strtoul(token, NULL, 0);
        printf("Memory length received from peer %lu\n", len);
        token = strtok(NULL, "-");
        addr = token;
        printf("Address received from peer: %s \n", token);
        sscanf(token, "%p", &addr);
        addr = mmap(addr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        printf("Address returned by mmap(): %p \n\n", addr);

        if (addr == MAP_FAILED) {
            errExit("error in address allocation in mmap ");
        }

    }

    /* Assignment 3 Part 2 Starts from here */

    char mode;
    int request_page, s;
    long uffd;
    struct uffdio_api uffdio_api;
    struct uffdio_register uffdio_register;
    pthread_t thr;
    char message[page_size];

    /* Registering userfaultfd file descriptor */

    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1) {
        errExit("userfaultfd");
    }
    uffdio_api.api = UFFD_API;
    uffdio_api.features = 0;
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1) {
        errExit("ioctl-UFFDIO_API");
    }
    uffdio_register.range.start = (unsigned long) addr;
    uffdio_register.range.len = len;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
        errExit("ioctl-UFFDIO_REGISTER");

    s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
    if (s != 0) {
        errno = s;
        errExit("pthread_create");
    }

    /* Starting user for operations */
    do {
        printf("> Which command should I run? (r:read, w:write): ");
        ignore_return(scanf(" %c", &mode));
        printf("\n> For which page? (0-%lu, or -1 for all): ", num_pages - 1);
        ignore_return(scanf(" %d", &request_page));

        if (mode == 'r') {
            printPageContents(addr, num_pages, request_page);
        } else if (mode == 'w') {
            printf("> Type your new message: \n");
            ignore_return(scanf(" %[^\n]", message));
            if (strlen(message) > page_size) {
                printf("The length of message exceeds page size. The message will be truncated \n");
            }
            updatePageContents(addr, request_page, num_pages, message);
            printPageContents(addr, num_pages, request_page);
        } else {
            printf("Please enter valid option : r or w");
        }
    } while (1);

    return 0;
}

static void *fault_handler_thread(void *arg) {
    static struct uffd_msg msg;
    long uffd;
    static char *page = NULL;
    struct uffdio_copy uffdio_copy;
    ssize_t nread;

    uffd = (long) arg;
    if (page == NULL) {
        page = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (page == MAP_FAILED)
            errExit("mmap");
    }

    for (;;) {

        struct pollfd pollfd;
        int nready;

        pollfd.fd = uffd;
        pollfd.events = POLLIN;
        nready = poll(&pollfd, 1, -1);
        if (nready == -1)
            errExit("poll");

        printf(" [x] PAGEFAULT\n");

        nread = read(uffd, &msg, sizeof(msg));
        if (nread == 0) {
            printf("EOF on userfaultfd!\n");
            exit(EXIT_FAILURE);
        }

        if (nread == -1)
            errExit("read");

        if (msg.event != UFFD_EVENT_PAGEFAULT) {
            fprintf(stderr, "Unexpected event on userfaultfd\n");
            exit(EXIT_FAILURE);
        }

        memset(page, '\0', page_size);
        uffdio_copy.src = (unsigned long) page;
        uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address & ~(page_size - 1);
        uffdio_copy.len = page_size;
        uffdio_copy.mode = 0;
        uffdio_copy.copy = 0;

        if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
            errExit("ioctl-UFFDIO_COPY");
    }
}
