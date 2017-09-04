

slimserver-fwd: slimserver-fwd.c
	$(CC) -Wall -Werror -o $@ -O1 $^

clean:
	rm slimserver-fwd
