CC=gcc
CFLAGS=-Wall -Wextra -I../inc -O3
LDFLAGS= -lwiringPi -lmosquitto
SOURCES=$(wildcard ../src/*.c)
OBJECTS=$(notdir $(SOURCES:.c=.o))
HEADERS=$(wildcard ../inc/*.h)
EXAMPLES = TecFanControl PeltierControl
EXECUTABLES=$(addsuffix .out, $(EXAMPLES))
OUTPUT_DIR=out

.PHONY: all
all: $(EXECUTABLES:%=$(OUTPUT_DIR)/%)

$(OUTPUT_DIR)/%.out: $(addprefix $(OUTPUT_DIR)/, $(OBJECTS)) $(OUTPUT_DIR)/%.o
	$(CC) $(addprefix $(OUTPUT_DIR)/, $(OBJECTS)) $(OUTPUT_DIR)/$*.o -o $@ $(LDFLAGS)

$(OUTPUT_DIR)/%.o: %.c | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR)/%.o: ../src/%.c | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

.PHONY: clean
clean:
	rm -rf $(OUTPUT_DIR)

.PHONY: format
format:
	clang-format -i -style=file $(EXAMPLES:=.c) $(SOURCES) $(HEADERS)
