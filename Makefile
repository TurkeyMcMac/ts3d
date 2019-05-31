exe = ts3d
sources = $(wildcard src/*.c)
headers = $(wildcard src/*.h)

cflags = -std=c99 -Wall -Wextra ${CFLAGS}
linkage = -lm -lncurses

$(exe): $(sources) $(headers)
	$(CC) $(cflags) -o $@ $(sources) $(linkage)

.PHONY: clean
clean:
	$(RM) $(exe)
