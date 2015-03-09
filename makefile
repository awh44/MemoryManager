PAGER=less
CC=gcc
OPTS=-o

AWK=awk -F " " '{ print $$NF }'

.PHONY: view-results run

view-results: output/myresults.txt
	$(PAGER) output/myresults.txt

compare: output/myresults.txt
	#Compare just the values (i.e., the rightmost field). Head removes the statistics from the end
	#of my results
	$(AWK) output/myresults.txt | head -n -3 | diff - output/values_correct.txt

output/myresults.txt: manager
	./manager input/addresses.txt input/BACKING_STORE.bin > output/myresults.txt

run: manager
	./manager $(FILE) $(BACKING)

manager: build/main.o build/lru_queue.o
	$(CC) $(DEBUG) $(OPTS)manager build/main.o build/lru_queue.o

build/main.o: src/main.c
	$(CC) -c $(DEBUG) $(OPTS)build/main.o src/main.c

build/lru_queue.o: include/lru_queue.h src/lru_queue.c
	$(CC) -c $(DEBUG) $(OPTS)build/lru_queue.o src/lru_queue.c

clean:
	rm -f manager
	rm -f build/*.o
	rm -f output/myresults.txt
