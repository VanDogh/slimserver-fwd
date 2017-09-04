
CFLAGS := -O1 -std=c99 -Wall -Werror

slimserver-fwd: slimserver-fwd.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	-rm -f slimserver-fwd
