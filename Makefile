
CFLAGS := -O1 -std=gnu99 -Wall -Werror

slimserver-fwd: slimserver-fwd.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	-rm -f slimserver-fwd
