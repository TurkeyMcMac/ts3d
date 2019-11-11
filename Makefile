version = 1.3.4
exe = ts3d
tests = tests
data-dir = data
man-page = ts3d.6
man-dir = /usr/share/man/man6
TS3D_ROOT = $(HOME)/.ts3d
test-log = tests.log
man-install = $(man-dir)/$(man-page).gz
data-install = $(TS3D_ROOT)/data
sources = src/*.c
headers = src/*.h

cflags = -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -DJSON_WITH_STDIO \
	 -DTS3D_VERSION='"$(version)"' ${CFLAGS}
linkage = -lm -lcurses
test-flags = -shared -fPIC -Og -g3 -DCTF_TESTS_ENABLED

CC ?= cc
RM ?= rm -f

$(exe): $(sources) $(headers)
	$(CC) $(cflags) -o $@ $(sources) $(linkage)

$(tests): $(sources) $(headers)
	$(CC) $(cflags) $(test-flags) -o $@ $(sources) $(linkage)

.PHONY: install
install: $(data-install) $(man-install)

$(data-install): $(data-dir)
	mkdir -p "$@"
	cp -fr $</* "$@"

$(man-install): $(man-page)
	sed '1s/@@VERSION@@/$(version)/' $< | gzip | sudo tee $@ >/dev/null

$(test-log): $(tests)
	ceeteef -s $(tests) > $(test-log)

.PHONY: run-tests
run-tests: $(test-log)
	cat $(test-log)

.PHONY: clean
clean:
	$(RM) $(exe) $(tests) $(test-log)
