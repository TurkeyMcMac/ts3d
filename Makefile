exe = ts3d
dev-exe = dev
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
version-header = src/version.h

cflags = -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200112L -DJSON_WITH_STDIO \
	 ${CFLAGS}
linkage = -lm -lcurses
test-flags = -fPIC -Og -g3 -DCTF_TESTS_ENABLED

CC ?= cc
RM ?= rm -f

$(dev-exe): $(sources) $(headers)
	./compile -t $@ -c $(CC) -b build/dev -j 4 -F $(cflags) -- -L $(linkage)

$(exe): $(sources) $(headers)
	./compile -t $@ -c $(CC) -b build/release -j 4 \
		-F $(cflags) -O2 -flto -- \
		-L $(linkage)

$(tests): $(sources) $(headers)
	./compile -t $@ -c $(CC) -b build/test -j 4 \
		-J $(CC) $(test-flags) -fPIC -o {o} {i} $(linkage) -- \
		-F $(cflags) $(test-flags)

$(version-header): $(version-file)
	echo "#define TS3D_VERSION \"$(version)\"" > $@

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
	ceeteef -t8 $(tests)

.PHONY: clean
clean:
	$(RM) -r $(exe) $(dev-exe) $(tests) build
