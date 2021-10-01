CC = gcc 
CFLAGS = -Wall -Wextra
LDFLAGS = -Wl,-rpath="$(CURDIR)" -L. -lcocoro

CURDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OUT ?= build
LIBCOCORO = libcocoro.so
TEST_1 = $(OUT)/test_1

GIT_HOOKS := .git/hooks/applied
LIB_OBJ = $(OUT)/coro.o $(OUT)/event.o $(OUT)/switch.o
TEST_1_OBJ = $(OUT)/test_1.o

all: CFLAGS += -O3 -g
all: $(GITHOOKS) $(LIBCOCORO) $(TEST_1)


$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

$(TEST_1): $(LIBCOCORO) $(TEST_1_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

$(LIBCOCORO): $(LIB_OBJ)
	$(CC) -shared $(LIB_OBJ) -o $@

$(OUT)/%.o: lib/%.c
	mkdir -p $(@D)
	$(CC) -fPIC -c $(CFLAGS) $< -o $@

$(OUT)/%.o: tests/%.c
	mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -I. $< -o $@

clean:
	$(RM) -rf build
	$(RM) -rf $(LIBCOCORO)
