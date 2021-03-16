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
#include <stdbool.h>

#define BUFF_SIZE 4096

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE);    \
    } while (0)
enum state {m, s, i};

static inline void ignore_return() {}

static void *fault_handler_thread(void *arg);
static void *responder(void *arg);
void updatePageContents(char *addr, int request_page, const char *message, bool transition);

static const int readOp = 1;
static const int invalidateOp = 2;

static char *baseAddr;

static int page_size, new_socket, remote_socket;
static unsigned long num_pages;
static enum state msi_array[100];

char * communicateWithPeer(int request_page, int operation) {
    char message[page_size];
    char * ret;
    sprintf(message, "%d-%d", request_page , operation);
    send(remote_socket, message, strlen(message), 0);
    int bytesRead = 0;
    memset(message, '\0', page_size);
    bytesRead = read(remote_socket, message, page_size);
    if (bytesRead < 0) {
        perror("Error in reading");
        exit(EXIT_FAILURE);
    } else if (bytesRead == 0) {
        printf("Read 0 bytes \n");
        message[0] = '\0';
    }
    ret = message;
    return ret;
}

void printPageContents(char *addr, int request_page, bool fetch) {
    char page_output[page_size + 1];
    char * message;
    for (int j = ((request_page == -1) ? 0 : request_page); j <= ((request_page == -1) ? (num_pages - 1) : request_page); ++j) {
        sprintf(page_output, "%s", addr + (j * page_size));
        enum state page_state = msi_array[j];
        switch (page_state) {
            case m:
            case s:
                // sprintf(page_output, "%s", addr + (j * page_size));
                break;
            case i:
                printf("[INFO]: State of page is I. Fetching from remote\n");
                char resp[] = "";
                if(fetch) {
                    message = communicateWithPeer(j, readOp);
                    if(strcmp("NULL", message)!=0) {
                        updatePageContents(addr, j, message, false);
                        msi_array[j] = s;
                    } else {
                        message = resp;
                    }
                } else message = resp;
                sprintf(page_output, "%s", message);
                break;
        }
        printf(" [*] Page %d:\n%s\n", j, page_output);
    }
}

void transitionStateOnWrite(int request_page) {
    enum state page_status = msi_array[request_page];
    switch (page_status) {
        case m:
        case s:
        case i:
            msi_array[request_page] = m;
            printf("[INFO]: Writing to Page %d, Status changed to M, Invalidated Peer cache \n", request_page);
            communicateWithPeer(request_page, invalidateOp);
            break;
    }
}

void updatePageContents(char *addr, int request_page, const char *message, bool transition) {
    int l;
    for (int i = ((request_page == -1) ? 0 : request_page); i <= ((request_page == -1) ? num_pages - 1 : request_page); ++i) {
        l = 0x0 + (i * page_size);
        for (int j = 0; message[j] != '\0'; ++j) {
            addr[l++] = message[j];
        }
        addr[l] = '\0';
        if (transition) {
            transitionStateOnWrite(i);
        }
    }
}

void fillWithInvalid(enum state * msiArray) {
    for (int j = 0; j <= num_pages; ++j) {
        msiArray[j] = i;
    }
}

char getMSItype(enum state value) {
    switch (value) {
        case m: return 'M';
        case s: return 'S';
        case i: return 'I';
        default : return '\0';
    }
}

void printMsiArray(enum state * msiArray) {
    for (int j = 0; j < num_pages; ++j) {
        printf(" [*] Page %d : %c \n", j, getMSItype(msiArray[j]));
    }
}

int main(int argc, char *argv[]) {
    int local_server_fd;
    struct sockaddr_in local_addr, remote_addr;
    int opt = 1, first_connection_failed = 0;
    int addrlen = sizeof(local_addr);
    char buffer[BUFF_SIZE] = {0};
    char *addr;
    unsigned long local_port, remote_port, len = 0;

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
    page_size = sysconf(_SC_PAGE_SIZE);

    /* If first_connection_failed, then this is the first process and it would ask for user input */
    if (first_connection_failed) {
        printf("How many pages would you like to allocate (greater than 0)? \n");
        if (scanf("%lu", &num_pages) == EOF) { perror("EOF on scanning input"); }
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
        printf("[INFO]: Memory length received from peer %lu\n", len);
        num_pages = len / page_size;
        token = strtok(NULL, "-");
        addr = token;
        printf("[INFO]: Address received from peer: %s \n", token);
        sscanf(token, "%p", &addr);
        addr = mmap(addr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        printf("[INFO]: Address returned by mmap(): %p \n\n", addr);

        if (addr == MAP_FAILED) {
            errExit("error in address allocation in mmap ");
        }

    }
    baseAddr = addr;

    /* Assignment 3 Part 2 Starts from here */

    char mode;
    int request_page, thread_ret;
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

    thread_ret = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
    if (thread_ret != 0) {
        errno = thread_ret;
        errExit("pthread_create");
    }


    fillWithInvalid(msi_array);
    pthread_t resp;
    thread_ret = pthread_create(&resp, NULL, responder, (void *) uffd);
    if (thread_ret != 0) {
        errno = thread_ret;
        errExit("pthread_create");
    }


    /* Starting user for operations */
    do {
        printf("> Which command should I run? (r:read, w:write, v:view msi array): ");
        ignore_return(scanf(" %c", &mode));
        if (mode != 'v') {
            printf("\n> For which page? (0-%lu, or -1 for all): ", num_pages - 1);
            ignore_return(scanf(" %d", &request_page));
        }

        if (mode == 'r') {
            printPageContents(addr, request_page, true);

        } else if (mode == 'w') {
            printf("> Type your new message: \n");
            ignore_return(scanf(" %[^\n]", message));
            if (strlen(message) > page_size) {
                printf("The length of message exceeds page size. The message will be truncated \n");
            }
            updatePageContents(addr, request_page, message, true);
            printPageContents(addr, request_page, false);

        } else if (mode == 'v') {
            printMsiArray(msi_array);
        } else {
            printf("Please enter valid option : r or w");
        }
    } while (1);

    return 0;
}

static void transitionStateOnRead(int request_page) {
    enum state page_status = msi_array[request_page];
    switch (page_status) {
        case m:
            msi_array[request_page] = s;
            break;
        case s:
        case i:
            break;
    }
}

static char *getResponse(int page) {
    enum state page_status = msi_array[page];
    switch (page_status) {
        case m:
            msi_array[page] = s;
        case s:
            return (baseAddr + (page * page_size));
        case i:
            return "NULL";
    }
    return '\0';
}

static void *responder(void *arg) {
    char buffer[page_size];
    int bytesRead = 0;
    for (;;) {
        memset(buffer, '\0', page_size);
        bytesRead = read(new_socket, buffer, page_size);
        if (bytesRead < 0) {
            perror("Error in reading");
            exit(EXIT_FAILURE);

        } else if (bytesRead > 0) {
            char message[page_size];

            char *token;
            token = strtok(buffer, "-");
            int request_page = atoi(token);
            token = strtok(NULL, "-");
            int operation = atoi(token);
            printf("\n[INFO]: Received Request for Page: %d , Operation(1:Read, 2:Invalidate): %d \n", request_page, operation);
            enum state initialState = msi_array[request_page];
            if (operation == invalidateOp) {
                msi_array[request_page] = i;
                char * page_addr = baseAddr + (request_page * page_size);
                if (madvise(page_addr, page_size, MADV_DONTNEED)) {
                    errExit("Fail to madvise");
                }
                snprintf(message, page_size, "Success");

            } else if (operation == readOp) {
                char * m = getResponse(request_page);
                transitionStateOnRead(request_page);
                snprintf(message, page_size, "%s", m);

            }
            printf("[INFO]: Page State was: %c . Now responding with : %s\n", getMSItype(initialState) ,message);
            send(new_socket, message, strlen(message), 0);
        }
    }
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
