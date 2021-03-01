## [M1]
# This gives the path of the current directory. 
# MAKEFILE_LIST Contains the name of each makefile that is parsed by make, in the order in which it was parsed. 
# From that we extract the last word and find the path of it. We execute 
#  ...
CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

## [M2]
#  Storing gcc command in CC variable and the execution flags in CFLAGS. This would be later used as to generate "action" to compile the source files.
#  ...
CC = gcc
CFLAGS += -g -O2 -Werror -Wall
LDFLAGS +=

## [M3] -- REMOVED
#  ...

## [M5]
#  Assigns a list of all the C source files in a directory to SRC_FILES. wildcard gives all the filenames of .c files
#  ...
SRC_FILES = $(wildcard *.c)

## [M6]
#  This renames the source files and removes the .c suffix, since the target executables would just have the filename.
# This is needed to build all source files into object files, and then add the object files to the archive
#  ...
EXE_FILES = $(SRC_FILES:.c=)

## [M7]
# This defines the dependencies of the "all" target. The EXE_FILES variable will have the executable filenames from above.
# 
#  ...
all: $(EXE_FILES)

## [M8] -- UPDATED
# %:%.c : This means all target files depend on its .c files
#  $@ means the filename of the target of the rule. Hence it specifies the filename of target after the source is compiled. $< means the first pre-requesite of the target. 
# The target action combination here creates the execution statement for compiling the source files
# With the % wilcard, this set if repeated for all the sources files, stitching the dependencies, it compiles all the source files and creates executable target binaries.
#  ...
%:%.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< 

## [M9]
# The target clean is added to remove the executable files. The action here removes the target files tagged under variable EXE_FILES.
# 
#  ...
clean:
	rm -f $(EXE_FILES)

## [M10]
#  .PHONY specifies that the targets all & clean are not files and are special targets. 
# Otherwise in a scenario where there are file also with filenames as all or clean, then it would cause conflicts. 
#  ...
.PHONY: all clean
