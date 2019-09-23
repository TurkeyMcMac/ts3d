exe = ts3d
tests = tests
test-log = tests.log
sources = src/*.c
headers = src/*.h

cflags = -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L ${CFLAGS}
linkage = -lm -lcurses -lpthread
test-flags = -shared -fPIC -Og -g3 -DCTF_TESTS_ENABLED

CC ?= cc
RM ?= rm -f

$(exe): $(sources) $(headers)
	$(CC) $(cflags) -o $@ $(sources) $(linkage)

$(tests): $(sources) $(headers)
	$(CC) $(cflags) $(test-flags) -o $@ $(sources) $(linkage)

$(test-log): $(tests)
	ceeteef -s $(tests) > $(test-log)

.PHONY: run-tests
run-tests: $(test-log)
	cat $(test-log)

.PHONY: clean
clean:
	$(RM) $(exe) $(tests) $(test-log)
