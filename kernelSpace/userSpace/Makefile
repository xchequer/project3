# Define FUSE VERSION and the like
FUSE_USE_VERSION = 29

# Add the names of your .c files from your assignment here.
SRCS = memefs.c
 
# This line is the name of the program you are building
BIN = memefs
 
# Compiler flags to pass to the system's C compiler while building
# the source files
CFLAGS = -g -Wall -std=gnu11 -I/usr/include/fuse -D_FILE_OFFSET_BITS=64
# Flags to pass to the linker while linking your program
# You shouldn't need to change this at all.
LDFLAGS = -g -lfuse
 
###################################################################
 # You shouldn't need to touch anything below this line...         #
###################################################################
OBJS = $(SRCS:.c=.o)
 
all: $(BIN)
 
%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(BIN): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	-rm -f $(OBJS)
	-rm -f $(BIN)

