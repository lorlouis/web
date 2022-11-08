SOURCE = main.c headers.c logging.c conn.c\
		 response_header.c
HEADER	=
SRC_DIR = src
BUILD_DIR = build
OUT	= sv
CC	= gcc
FLAGS = -c -Wall -fanalyzer
LFLAGS = -lssl -lcrypto -lmagic

OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCE))

all: $(OBJS)
	$(CC) -o $(OUT) $^ $(LFLAGS)

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(BUILD_DIR)
	$(CC) $(FLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(OUT)

git_init:
	git submodule update --init --recursive
