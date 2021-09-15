ENTRYPOINT = main.c
_ENTRYPOINT_O = main.o
SOURCE = headers.c mimes.c
_OBJS  = headers.o mimes.o
HEADER =
CC = gcc
FLAGS = -g -c -Wall -fanalyzer
LFLAGS =
BUILD_DIR = build
MKDIR_P = mkdir -p


_TESTS_ENTRYPOINT_O = tests.o
TESTS_SOURCE = tests.c
_TESTS_OBJS =
TESTS_OUT = test

OUT = sv


ENTRYPOINT_O = $(patsubst %,$(BUILD_DIR)/%,$(_ENTRYPOINT_O))

OBJS = $(patsubst %,$(BUILD_DIR)/%,$(_OBJS))

TESTS_ENTRYPOINT_O = $(patsubst %,$(BUILD_DIR)/%,$(_TESTS_ENTRYPOINT_O))

TESTS_OBJS = $(patsubst %,$(BUILD_DIR)/%,$(_TESTS_OBJS))

debug: $(ENTRYPOINT_O) $(OBJS)
	$(CC) -g $(ENTRYPOINT_O) $(OBJS) -o $(OUT) $(LFLAGS)


tests: $(TESTS_OUT)
	./$(TESTS_OUT)

$(TESTS_OUT): $(TESTS_ENTRYPOINT_O) $(TESTS_OBJS) $(OBJS)
	$(CC) -g $(TESTS_ENTRYPOINT_O) $(TESTS_OBJS) $(OBJS) -o $(TESTS_OUT) $(LFLAGS)

rel: $(ENTRYPOINT_O) $(OBJS)
	$(CC) -O3 $(OBJS) -o $(OUT) $(LFLAGS)

$(BUILD_DIR)/%.o: %.c
	$(MKDIR_P) $(BUILD_DIR)
	$(CC) $(FLAGS) $< -o $@


clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OUT)
	rm -f $(TESTS_OUT)
