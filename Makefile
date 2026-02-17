VERSION := $(shell cat VERSION)
CXX := cc
CXXFLAGS := -DVERSION=\"${VERSION}\" -O3 -Wall -Wextra -Iinclude

SRC_DIR := src
BUILD_DIR := build
BIN := $(BUILD_DIR)/kiri

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/%, $(SOURCES:.c=.o))
OBJECTS := $(OBJECTS:.c=.o)

$(BIN): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean run

install: $(BIN)
	cp ./$(BIN) ~/.local/bin/kiri

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BUILD_DIR)

