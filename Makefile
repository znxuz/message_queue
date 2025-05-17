SRC_DIR := .
BUILD_DIR := build
DIR_GUARD = @mkdir -p "$(@D)"

CC := g++
LANG := c++
STD := c++23
WARNINGS := -Wall -Wextra -Wpedantic -Weffc++ -Wnrvo -Wconversion \
			-Wnull-dereference -Wnon-virtual-dtor -Wcast-align \
			-Woverloaded-virtual -Wshadow -Wsequence-point \
			-Wno-unused-parameter -Wno-unused-function -Wno-unused-variable
CFLAGS := -x $(LANG) -std=$(STD) $(WARNINGS) -O0 -I$(CURDIR)
LIB :=
SAN := -fsanitize=address,undefined

src := $(filter-out ./tests/tests.cpp, $(shell find $(SRC_DIR) -type f -name "*.cpp"))
obj := $(addprefix $(BUILD_DIR)/, $(src:.cpp=.o))
NAME := $(BUILD_DIR)/a.out

.PHONY: compile debug

all: compile

compile: $(NAME)

recompile:
	$(MAKE) clean
	$(MAKE) compile

run: $(NAME)
	@./$(NAME)

rerun:
	$(MAKE) clean
	$(MAKE) run

debug: CFLAGS += -g -DDEBUG=1 -D_GLIBCXX_DEBUG=1
debug: fclean all

$(NAME): $(obj)
	$(CC) $(obj) $(LIB) $(SAN) -o $(NAME)

$(BUILD_DIR)/%.o: %.cpp
	$(DIR_GUARD)
	$(CC) $(CFLAGS) -c $^ -o $@

leak: $(NAME)
	valgrind --leak-check=full $(NAME)

clean:
	$(RM) $(obj)

fclean: clean
	$(RM) -r $(BUILD_DIR)

clangdb: clean
	@mkdir -p $(BUILD_DIR)
	@compiledb --output $(BUILD_DIR)/compile_commands.json -- $(MAKE) -j
