exe = ts3d
exe-dir = $(HOME)/bin
version-file = version
version = `cat $(version-file)`
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

cflags = -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200112L -DJSON_WITH_STDIO \
	 -DTS3D_VERSION="\"$(version)\"" ${CFLAGS}
linkage = -lm -lcurses
test-flags = -shared -fPIC -Og -g3 -DCTF_TESTS_ENABLED \
	-U_POSIX_C_SOURCE -D_POSIX_C_SOURCE=200809L # tests need POSIX 2008.

CC ?= cc
RM ?= rm -f

$(exe): $(sources) $(headers) $(version-file)
	$(CC) $(cflags) -o $@ $(sources) $(linkage)

$(tests): $(sources) $(headers)
	$(CC) $(cflags) $(test-flags) -o $@ $(sources) $(linkage)

.PHONY: install
install: $(exe)
	MAKEFILE=yes VERSION=$(version) \
	EXE="$(exe)" EXE_INSTALL="$(exe-install)" \
	DATA_DIR="$(data-dir)" DATA_INSTALL="$(data-install)" \
	MAN_PAGE="$(man-page)" MAN_INSTALL="$(man-install)" \
	./installer install

.PHONY: uninstall
uninstall:
	MAKEFILE=yes VERSION=$(version) \
	EXE="$(exe)" EXE_INSTALL="$(exe-install)" \
	DATA_DIR="$(data-dir)" DATA_INSTALL="$(data-install)" \
	MAN_PAGE="$(man-page)" MAN_INSTALL="$(man-install)" \
	./installer uninstall

.PHONY: run-tests
run-tests: $(tests)
	ceeteef $(tests)

.PHONY: clean
clean:
	$(RM) $(exe) $(tests)
