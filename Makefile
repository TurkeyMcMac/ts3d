version = 1.3.5
exe = ts3d
exe-dir = $(HOME)/bin
tests = tests
data-dir = data
man-page = ts3d.6
man-dir = /usr/share/man/man6
TS3D_ROOT ?= $(HOME)/.ts3d
TS3D_DATA ?= $(TS3D_ROOT)/data
exe-install = $(exe-dir)/$(exe)
man-install = $(man-dir)/$(man-page).gz
data-install = $(TS3D_DATA)
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
install: $(exe-install) $(data-install) $(man-install)

$(exe-install): $(exe)
	cp $< "$@"

$(data-install): $(data-dir)
	mkdir -p "$@"
	cp -fr $</* "$@"

$(man-install): $(man-page)
	sed '1s/@@VERSION@@/$(version)/' $< | gzip > $@

.PHONY: uninstall
uninstall:
	$(RM) -r $(exe-install) $(man-install) $(data-install)
	rmdir "$(TS3D_ROOT)" 2>/dev/null || : ignore error

.PHONY: run-tests
run-tests: $(tests)
	ceeteef $(tests)

.PHONY: clean
clean:
	$(RM) $(exe) $(tests)
