/* userfaultfd_demo.c

   Licensed under the GNU General Public License version 2 or later.
*/
#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
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

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE);	\
	} while (0)

static int page_size;

static void *
fault_handler_thread(void *arg)
{
	static struct uffd_msg msg;   /* Data read from userfaultfd */
	static int fault_cnt = 0;     /* Number of faults so far handled */
	long uffd;                    /* userfaultfd file descriptor */
	static char *page = NULL;
	struct uffdio_copy uffdio_copy;
	ssize_t nread;

	uffd = (long) arg;

	/* [H1]
	 * mmap() creates a mapping in a virtual memory of length equivalent to page_size parameter.
	 * mmap creates a new mapping in the virtual address space of this process. The prot flags here enables that pages may be read and written. 
	 * The flag MAP_PRIVATE Creates a private copy-on-write mapping, updates to the mapping are not visible to other processes mapping the same file
	 * Since we are using MAP_ANONYMOUS, the mapping is not stored in any file and the contents are initialised by zero, 
	 * Hence the fd argument is ignored and hence sent to -1 and offset value is also set to 0. We are not providing any starting address to mmap, hence the kernel chooses the address at which to create the mapping.
	 * If returned value is MAP_FAILED, then that means address mapping creation failed. On success, the address pointer to the mapped area is returned and stored in addr. 
	 * The virtual address space mapped here is to be used to copy chunk of data to the faulting region address space in the later section of this routine.
	 */
	if (page == NULL) {
		page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (page == MAP_FAILED)
			errExit("mmap");
	}

	/* [H2]
	 * This is an infinite loop, the statements inside the section are repeatedly executed in the thread.
 	 * In the context of program, the below section continuously polls userfaultfd file descriptor, allowing userland to pass on instructions in events of page faults.
	 */
	for (;;) {

		/* See what poll() tells us about the userfaultfd */

		struct pollfd pollfd;
		int nready;

		/* [H3]
		 * We are creating a poll file descriptor which we are using to monitor the poll.
		 POLLIN flag means there is data to read, which specifies the event we are interested in.
		 Poll function keeps on polling, till any of file descriptors gets ready for any IO operation, here specifically returns number of file descriptors which are ready to be read.
		 The timeout value here is negative, hence the timeout set here in infinite interval, which means the 
		 poll function would continue to block waiting till the file descriptor is ready.
		 Once we get a return value, we print all the details here.
		 */
		pollfd.fd = uffd;
		pollfd.events = POLLIN;
		nready = poll(&pollfd, 1, -1);
		if (nready == -1)
			errExit("poll");

		printf("\nfault_handler_thread():\n");
		printf("    poll() returns: nready = %d; "
                       "POLLIN = %d; POLLERR = %d\n", nready,
                       (pollfd.revents & POLLIN) != 0,
                       (pollfd.revents & POLLERR) != 0);

		/* [H4]
		 * We are reading bytes (of length up to the size of msg) from the userfaultfd file descriptor. 
		 * The read data is stored in uffd_msg structure.
		 * read returns the number of bytes read. If it returns 0, it means we it reached end of file.
		 */
		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) {
			printf("EOF on userfaultfd!\n");
			exit(EXIT_FAILURE);
		}

		if (nread == -1)
			errExit("read");

		/* [H5]
		 * UFFD_EVENT_PAGEFAULT is a page-fault event flag. The event flags in message read above is compared with the flag
		 If the event in the read message is not a page fault event, then the program exits.
		 */
		if (msg.event != UFFD_EVENT_PAGEFAULT) {
			fprintf(stderr, "Unexpected event on userfaultfd\n");
			exit(EXIT_FAILURE);
		}

		/* [H6]
		 * Prints the results of a Page fault events with flags along with the address.
		 */
		printf("    UFFD_EVENT_PAGEFAULT event: ");
		printf("flags = %llx; ", msg.arg.pagefault.flags);
		printf("address = %llx\n", msg.arg.pagefault.address);

		/* [H7]
		 * We are storing the page fault count in fault_cnt integer variable. For every page fault we increment the counter by one.
		 * Now after every page fault, we set the character starting from A (and incrementing it for every page fault till first 20 alphabets) to all memory locations pointed by variable page. 
		 */
		memset(page, 'A' + fault_cnt % 20, page_size);
		fault_cnt++;

		/* [H8]
		 * We are initialising the source and destination address into the data structure.
		 * We are handling page faults in units of page sizes. Hence we are rounding faulting address down to page boundary.
		 */
		uffdio_copy.src = (unsigned long) page;
		uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
			~(page_size - 1);
		uffdio_copy.len = page_size;
		uffdio_copy.mode = 0;
		uffdio_copy.copy = 0;

		/* [H9]
		 * UFFDIO_COPY ioctl atomically copies a continuous memory chunk into the userfault registered range. 
		 * Here in this operation, this thread is copying a page of data into the faulting region using the UFFDIO_COPY ioctl.
		 */
		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
			errExit("ioctl-UFFDIO_COPY");

		/* [H10]
		 * The copy field stores the number of bytes that was actually copied.  Hence here we print the bytes which were copied.
		 */
		printf("        (uffdio_copy.copy returned %lld)\n",
                       uffdio_copy.copy);
	}
}

int
main(int argc, char *argv[])
{
	long uffd;          /* userfaultfd file descriptor */
	char *addr;         /* Start of region handled by userfaultfd */
	unsigned long len;  /* Length of region handled by userfaultfd */
	pthread_t thr;      /* ID of thread that handles page faults */
	struct uffdio_api uffdio_api;
	struct uffdio_register uffdio_register;
	int s;
	int l;

	/* [M1]
	 * This snippet ensures correct number of command line argument is passed as the input to program.
	 * The program takes in count of number of pages as input. If the argument is not passed or there are other number of arguments
	 * its prints out the program usage guideline and exits the program.
	 */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s num-pages\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* [M2]
	 * sysconf gets the system configuration at the runtime. Here we are fetching the page size at runtime. 
	 * sysconf(_SC_PAGE_SIZE) returns the number of bytes in a memory page. Page here is a unit of memory allocation and is a fixed length block.
	 * strtoul converts string to long. Hence, multiplying input argument with page size, we calculating the total length.  
	 */
	page_size = sysconf(_SC_PAGE_SIZE);
	len = strtoul(argv[1], NULL, 0) * page_size;

	/* [M3]
	 * This creates and enabled a userfaultfd file descriptor.
	 * If -1 is returned then it means an error occured and then errExit puts out the message in error stream and exits the program with error code.
	 * There are multiple falgs being input to the creation of the file descriptor to ensure the properties set to it. 
	 * O_CLOEXEC flag enables the close-on-exec flag for the new file descriptor.
	 * O_NONBLOCK : This allows opening the file descriptor in a non blocking way. 
 	 * Hence, This does cause other process to wait on any IO operation on the file descriptor.
	 */
	uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if (uffd == -1)
		errExit("userfaultfd");

	/* [M4]
	 * uffdio_api : This api enable operations of the userfaultfd and perform API handshake.
	 * Here we are setting API protocol to UFFD_API and features to default. 
	 * We can request for specific features by setting its bits. 
	 * The ioctl system call manipulates the underlying device parameters of uffd here with the api flag and features set above.
	 */
	uffdio_api.api = UFFD_API;
	uffdio_api.features = 0;
	if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
		errExit("ioctl-UFFDIO_API");

	/* [M5]
	 * mmap creates a new mapping in the virtual address space of this process.
	 * The prot flags here enables that pages may be read and written. 
	 * The flag MAP_PRIVATE Creates a private copy-on-write mapping, updates to the mapping are not visible to other processes mapping the same file
	 * Since we are using MAP_ANONYMOUS, the mapping is not stored in any file and the contents are initialised by zero, 
	 * Hence the fd argument is ignored and hence sent to -1 and offset value is also set to 0.
	 * We are not providing any starting address to mmap, hence
	 * the kernel chooses the address at which to create the mapping.
	 * If returned value is MAP_FAILED, then that means address mapping creation failed
	 * On success, the address pointer to the mapped area is returned and stored in addr.
	 */
	addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED)
		errExit("mmap");

	printf("Address returned by mmap() = %p\n", addr);

	/* [M6]
	 * the ioctl operation here Registers the memory address range with the userfaultfd object.
	 The range field defines a memory range starting at start and continuing for len bytes that should be handled by the
       userfaultfd.
	 The mode field defines the mode of operation desired for this memory region. 
	 UFFDIO_REGISTER_MODE_MISSING instructs kernel to Track page faults on missing pages.
	 */
	uffdio_register.range.start = (unsigned long) addr;
	uffdio_register.range.len = len;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
		errExit("ioctl-UFFDIO_REGISTER");

	/* [M7]
	 * pthread_create() function call here starts a new thread with default attributes which in turn invokes fault_handler_thread() routine here. 
	 * uffd is passed as the argument to the function which is the userfaultfd file descripter here.
	 So basically, in the program context we are spawning a thread to asynchronously poll the file descriptor and check for page faults in the background.
	 * On success, pthread_create() returns 0. Incase creation fails, it returns an error number and hence we are checking the value in s for 0.
	 */
	s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
	if (s != 0) {
		errno = s;
		errExit("pthread_create");
	}

	/*
	 * [U1]
	 * mmap function created a memory address in virtual address, but we didn't put anything in the physical memory.
	 * As soon as we access the memory location by addr[l], it causes a page fault. Now the section in fault_handler_thread() for handling page fault is executed.
	 * As per fault_handler_thread(), since this is the first page fault, the fault_cnt is 0 initially, and the routine sets 'A' to all the memory addresses, which gets printed by the snippet blow.
	 * l is like a offset counter here which we iterate over the total length of memory address in skips of 1024 bytes. 
	 * Then we read the character at that memory location, print the value and increment the counter by 1024.
	 * Hence the snippet as a whole iterates the mapped memory address in intervals of 1024 and accesses the stored data. 
	 * Since page size if 4096, and while running it for 1 page, we get character A printed 4 times.
	 
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#1. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U2]
	 * Here since there has been no memory allocation changes, hence no page fault occurs. 
	 * Hence, when we read the address again, we do not get any page fault and it prints 'A' exactly as above.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#2. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U3]
	 * With reference to documentations, the madvise() system call is used to give advice or directions to the kernel about the address range beginning at address addr and with size len bytes.
        	MADV_DONTNEED flag means : Do not expect access in the near future. On success, madvise() returns zero.
	 * HENCE, here we are asking the kernel to free up the memory address specified in addr, which was our mapped address.
	 * Post that, we try to access the memory in intervals of 1024 bytes, it causes page fault, and repopulation of the memory contents from the up-to-date  contents  of  the  underlying  mapped  file .
	 * Now fault_cnt is 1 from the previous page fault which results in setting character 'B' across all memory locations by the thread running fault_handler_thread() routine. 
	 * Hence we get character B printed across 4 memory locations in the interval of 1024.
	 */
	printf("-----------------------------------------------------\n");
	if (madvise(addr, len, MADV_DONTNEED)) {
		errExit("fail to madvise");
	}
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#3. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U4]
	 * We again revisit the address here and print the data stored in memory locations in intervals of 1024 bytes.
	 * Here since there has been memory mapping changes, hence no page fault occurs  it prints 'B' exactly as above.. 
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#4. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U5]
	 * We make madvise() call again which instructs kernel to free memory immediately. After a successful MADV_DONTNEED operation, 
	 * the semantics of memory access in  the specified  region  are  changed, subsequent  accesses  of  pages causes page fault 
	 * and repopulation of the memory contents from the up-to-date  contents  of  the  underlying  mapped  file
	 * Now explicitly we set the memory address with character '@' using the memset command,
	 * memset also internally access the memory address, which triggers the setting of alphabet characters in the mapped address (based on fault_cnt).
	 * But post that memset is executed which sets @ character to the memory addresses. Hence finally on printing addr[l] character, we get @.
	 */
	printf("-----------------------------------------------------\n");
	if (madvise(addr, len, MADV_DONTNEED)) {
		errExit("fail to madvise");
	}
	l = 0x0;
	while (l < len) {
		memset(addr+l, '@', 1024);
		printf("#5. write address %p in main(): ", addr + l);
		printf("%c\n", addr[l]);
		l += 1024;
	}

	/*
	 * [U6]
	 * We reiterate over memory address. There were no changes to mapped memory address, and also no deallocation of page, 
 	 * Hence, We do not observe any page fault and it prints the character @ for the multiple iterations on the counter (Basically len/1024 times).
	 *  
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#6. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U7]
	 * Here we are writing character ^ at all memory addresses in chunks of 1024 bytes using memset. There was no madvise() call, hence no alterations to page mapping. 
	 * The memor is still intact and hence, there are no page fault observed. Since there are no page fault, the logic inside fault_handler_thread() to update with alphabet based on fault count is not invoked.
	 * We print the addr[i], to show it prints ^ when the counter is iterated.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		memset(addr+l, '^', 1024);
		printf("#7. write address %p in main(): ", addr + l);
		printf("%c\n", addr[l]);
		l += 1024;
	}

	/*
	 * [U8]
	 * We reiterate over memory address staring from base address with step of 1024th location. Since, there were no changes to mapped memory address and also no deallocation of page, 
 	 * we do not observe any page fault and it prints the character ^ for the multiple iterations on the counter.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#8. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	exit(EXIT_SUCCESS);
}
