BUILD_DIR := build
DIR_GUARD = @mkdir -p "$(@D)"

MAIN_TEST := test.bin

CC := clang++
LANG := -x c++
STD := -std=c++23
WARNINGS := -Wall -Wextra -Wpedantic -Weffc++ -Wconversion -Wshadow \
	    -Wnon-virtual-dtor -Wcast-align -Woverloaded-virtual \
	    -Wnull-dereference -Wno-unused-parameter -Wno-unused-function \
	    -Wno-unused-variable
OPTM := -O0
INCL := -I../

CFLAGS := $(LANG) $(STD) $(WARNINGS) $(OPTM) $(INCL) -g

LD := clang++
LIB :=
SAN := -fsanitize=address,undefined
LDFLAGS := $(SAN) $(LIB) -lgtest

TEST_ARGUMENTS := --gtest_filter=*

test_header := $(shell grep -roh '\.\./src/.*\.hpp' . | sort | uniq | sed 's;\.\./;;')
test_src := $(shell find $(TEST_DIR) -type f -name "*.cpp") \
	    $(test_header:.hpp=.cpp)
test_obj := $(addprefix $(BUILD_DIR)/, $(test_src:.cpp=.o))

.PHONY: test clean leak

all: $(test_obj)

retest:
	$(MAKE) clean
	$(MAKE) test

test: $(BUILD_DIR)/$(MAIN_TEST)
	./$(BUILD_DIR)/$(MAIN_TEST) $(TEST_ARGUMENTS)

$(BUILD_DIR)/$(MAIN_TEST): $(test_obj)
	$(DIR_GUARD)
	$(LD) $(test_obj) $(LDFLAGS) -o $(BUILD_DIR)/$(MAIN_TEST)

$(BUILD_DIR)/%.o: %.cpp
	$(DIR_GUARD)
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	$(RM) $(test_obj)
	$(RM) $(BUILD_DIR)/$(MAIN_TEST)

clangdb: clean
	@mkdir -p $(BUILD_DIR)
	@compiledb --output $(BUILD_DIR)/compile_commands.json -- $(MAKE) -j
