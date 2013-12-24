# Assumes that the environment defines path to GCC tool chain
# in PATH variable ARM_TOOL_PATH

TOOL_CHAIN = arm-none-eabi-

# source files.
SRC :=  src/arm_btrace.c src/asm_utils.c

OBJ := $(SRC:.c=.o)

OUT = ./libbtrace.a

# include directories
INCLUDES = -I. -I./inc -I../cmn/inc

# C compiler flags (-g -O2 -Wall)
CFLAGS = -g

# compiler
CC = $(ARM_TOOL_PATH)/$(TOOL_CHAIN)gcc

# compile flags
LDFLAGS = -g

.SUFFIXES: .c .o

default: $(OUT)

$(OUT): $(OBJ)
	ar rcs $(OUT) $(OBJ)

src/%.o : src/%.c
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ -c $^

clean:
	rm -f $(OBJ) $(OUT) Makefile.bak 
