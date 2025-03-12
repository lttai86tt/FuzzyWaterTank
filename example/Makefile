CC=gcc
CFLAGS=-Wall -Wextra -I../inc -O3
LDFLAGS=
SOURCES=$(wildcard ../src/*.c)
OBJECTS=$(notdir $(SOURCES:.c=.o))
HEADERS=$(wildcard ../inc/*.h)
EXAMPLES = PeltierControl TecFanControl
EXECUTABLES=$(addsuffix .out, $(EXAMPLES))
OUTPUT_DIR=out

.PHONY: all
all: $(EXECUTABLES:%=$(OUTPUT_DIR)/%)

$(OUTPUT_DIR)/%.out: $(addprefix $(OUTPUT_DIR)/, $(OBJECTS)) $(OUTPUT_DIR)/%.o
	$(CC) $(LDFLAGS) $(addprefix $(OUTPUT_DIR)/, $(OBJECTS)) $(OUTPUT_DIR)/$*.o -o $@

$(OUTPUT_DIR)/%.o: %.c | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR)/%.o: ../src/%.c | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

.PHONY: clean
clean:
	if exist $(OUTPUT_DIR) rmdir /S /Q $(OUTPUT_DIR)

.PHONY: format
format:
	clang-format -i -style=file $(EXAMPLES:=.c) $(SOURCES) $(HEADERS)
