#include <stdio.h>
#include <sys/syscall.h>
#include <linux/kernel.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char * const argv[]) {
  char str[100];
  int key = 0, option;

  while ((option = getopt(argc, argv, "s:k:")) != -1) {
    switch (option) {
    case 'k':
      key = atoi(optarg);
      break;
    case 's':
      strcpy(str, optarg);
      break;
    case '?':
      printf("Unknown option: %c\n", optopt);
      break;
    }
  }
  printf("Input String is %s and key is %d \n", str, key);
  long ret = syscall(440, str, key);
  printf("Return code from syscall : %ld \n", ret);
  return 0;
}