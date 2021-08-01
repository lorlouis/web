SOURCE = main.c
_OBJS = main.o
HEADER =
CC = gcc
FLAGS = -g -c -Wall
LFLAGS =
BUILD_DIR = build
MKDIR_P = mkdir -p

OUT = sv
OBJS = $(patsubst %,$(BUILD_DIR)/%,$(_OBJS))

debug: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

rel: $(OBJS)
	$(CC) -O3 $(OBJS) -o $(OUT) $(LFLAGS)

$(BUILD_DIR)/%.o: %.c
	$(MKDIR_P) $(BUILD_DIR)
	$(MKDIR_P) $(BUILD_DIR)/lib
	$(CC) $(FLAGS) $< -o $@


clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OUT)
