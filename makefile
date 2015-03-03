PAGER=less
CC=gcc
OPTS=-o

view-results: output/myresults.txt
	$(PAGER) output/myresults.txt

output/myresults.txt: manager
	./manager input/addresses.txt input/BACKING_STORE.bin > output/myresults.txt

run: manager
	./manager $(FILE) $(BACKING)

manager: src/main.c
	$(CC) $(OPTS)manager src/main.c