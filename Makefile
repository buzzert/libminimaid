CC=gcc
CCFLAGS=-g
LIBS=-lusb-1.0

test: minimaid.o test.c
	$(CC) -o minimaidtest test.c minimaid.o $(CFLAGS) $(LIBS)

minimaid.o: minimaid.c 
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o minimaidtest
