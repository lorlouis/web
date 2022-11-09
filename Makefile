ENTRYPOINT = main.c
TEST_ENTRYPOINT = main.c
SOURCE = headers.c logging.c conn.c\
		 response_header.c
HEADER	=
SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build
OUT	= sv
CC	= gcc
FLAGS = -c -Wall -fanalyzer
LFLAGS = -lssl -lcrypto -lmagic

OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCE))

ENTRYPOINT_O = $(patsubst %.c,$(BUILD_DIR)/%.o,$(ENTRYPOINT))
TEST_ENTRYPOINT_O = $(patsubst %.c,$(TEST_DIR)/%.o,$(TEST_ENTRYPOINT))

all: $(OBJS) $(ENTRYPOINT_O)
	$(CC) -o $(OUT) $^ $(LFLAGS)

.PHONY: tests
tests: test
.PHONY: test
test: clean run_tests

run_tests: $(TEST_ENTRYPOINT_O)
	$(CC) -o unit_tests $^ $(LFLAGS)
	./unit_tests

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(BUILD_DIR)
	$(CC) $(FLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(OUT) $(TEST_ENTRYPOINT_O)
	rm -f unit_tests

git_init:
	git submodule update --init --recursive
