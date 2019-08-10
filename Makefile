exe = ts3d
tests = tests
test-log = tests.log
sources = $(wildcard src/*.c)
headers = $(wildcard src/*.h)

cflags = -std=c99 -Wall -Wextra ${CFLAGS}
linkage = -lm -lncurses
test-flags = -shared -Og -g3 -DCTF_TESTS_ENABLED

$(exe): $(sources) $(headers)
	$(CC) $(cflags) -o $@ $(sources) $(linkage)

$(tests): $(sources) $(headers)
	$(CC) $(test-flags) $(cflags) -o $@ $(sources) $(linkage)

$(test-log): $(tests)
	ceeteef -s $(tests) > $(test-log)

.PHONY: run-tests
run-tests: $(test-log)
	cat $(test-log)

.PHONY: clean
clean:
	$(RM) $(exe) $(tests)
