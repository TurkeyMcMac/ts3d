# See the recipes for $(dev-exe), $(exe), and $(tests) for jdibs examples.

exe = ts3d
dev-exe = dev
exe-dir = $(HOME)/bin
version-file = version
version = `cat $(version-file)`
tests = tests
windows-zip = ts3d.zip
data-dir = data
man-page = ts3d.6
man-dir = /usr/share/man/man6
TS3D_ROOT ?= $(HOME)/.ts3d
TS3D_DATA ?= $(TS3D_ROOT)/data
exe-install = $(exe-dir)/$(exe)
man-install = $(man-dir)/$(man-page).gz
data-install = $(TS3D_DATA)
sources = src/*.c
# Contains the version of the program:
version-header = src/version.h
# src/*.h will not include src/version.h if it has not been made yet:
headers = src/*.h $(version-header)
man-page-input = $(man-page).in

cflags = -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200112L -DJSON_WITH_STDIO \
	 ${CFLAGS}
linkage = -lm -lcurses
test-flags = -fPIC -O0 -g3 -DCTF_TESTS_ENABLED

CC ?= cc
RM ?= rm -f

# Fast unoptimized development build. The Makefile also has to know about the
# dependency of $(dev-exe) on things in the source directory. The Makefile, not
# ./compile, does the automatic generation of version.h.
$(dev-exe): $(sources) $(headers)
	@# -t sets the output destination.
	@# -c sets the compiler.
	@# -b sets the build directory (which will be automatically created.)
	@# -j sets the number of compilation processes in parallel.
	@# -F sets the flags to pass to every compiler invocation. The list is
	@#    terminated by '--'.
	@# -L sets the linking arguments passed after the sources when linking.
	@#    It is not terminated by '--' since the argument list ends here.
	./compile -t $@ -c $(CC) -b build/dev -j 4 -F $(cflags) -- -L $(linkage)

# Optimized build.
$(exe): $(sources) $(headers)
	./compile -t $@ -c $(CC) -b build/release -j 4 \
		-F $(cflags) -O2 -flto -- \
		-L $(linkage)

# Tests binary (running tests requires c-test-functions.)
$(tests): $(sources) $(headers)
	@# -J overrides the entire argument vector for the linking stage. Here
	@#    it is used to produce a shared object. The {o} will be replaced
	@#    with the output and the {i} will be replaced with all input files.
	./compile -t $@ -c $(CC) -b build/test -j 4 \
		-J $(CC) $(test-flags) -shared -o {o} {i} $(linkage) -- \
		-F $(cflags) $(test-flags)

$(version-header): $(version-file)
	echo "#define TS3D_VERSION \"$(version)\"" > $@

$(windows-zip): $(exe)
	./zip-windows

$(man-page): $(man-page-input) $(version-file)
	sed "s/@@VERSION@@/$(version)/g" $< | gzip > $@

.PHONY: install
install: $(exe) $(man-page)
	MAKEFILE=yes EXE="$(exe)" EXE_INSTALL="$(exe-install)" \
	DATA_DIR="$(data-dir)" DATA_INSTALL="$(data-install)" \
	MAN_PAGE="$(man-page)" MAN_INSTALL="$(man-install)" \
	./installer install

.PHONY: uninstall
uninstall:
	MAKEFILE=yes EXE_INSTALL="$(exe-install)" \
	DATA_INSTALL="$(data-install)" MAN_INSTALL="$(man-install)" \
	./installer uninstall

.PHONY: run-tests
run-tests: $(tests)
	ceeteef -t8 $(tests)

.PHONY: clean
clean:
	$(RM) -r $(exe) $(dev-exe) $(tests) $(windows-zip) $(man-page) \
	       	$(version-header) build
