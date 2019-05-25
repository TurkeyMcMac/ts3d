exe = ts3d
sources = $(wildcard src/*.c)
headers = $(wildcard src/*.h)

cflags = -std=c99 -DJSON_WITH_STDIO ${CFLAGS} 
linkage = -lm

$(exe): $(sources) $(headers)
	$(CC) $(cflags) -o $@ $(sources) $(linkage)

.PHONY: clean
clean:
	$(RM) $(exe)
