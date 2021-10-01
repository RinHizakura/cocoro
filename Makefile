CC = gcc 
CFLAGS = -Wall -Wextra -O3 

OUT ?= build
TEST_1 = $(OUT)/test_1

GIT_HOOKS := .git/hooks/applied
LIB_OBJ = $(OUT)/coro.o $(OUT)/event.o $(OUT)/switch.o
TEST_1_OBJ = $(OUT)/test_1.o

all: CFLAGS += -O3 -g
all: LDFLAGS += -O3
all: $(TEST_1)


$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

$(TEST_1): $(GIT_HOOKS) $(LIB_OBJ) $(TEST_1_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(LIB_OBJ) $(TEST_1_OBJ) $(LDFLAGS)

$(OUT)/%.o: lib/%.c
	mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $< -o $@ 

$(OUT)/%.o: tests/%.c
	mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -Ilib $< -o $@

clean:
	$(RM) -rf build
