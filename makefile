PAGER=less
CC=gcc
OPTS=-o

AWK=awk -F " " '{ print $$NF }'

.PHONY: view-results run manager make-results compare-orig compare-reduced compare-writeback

view-results: 
	$(PAGER) output/my_orig_results.txt output/my_reduced_results.txt output/my_writeback_results.txt

make-results: manager
	./run_script.bash 128 > output/my_orig_results.txt
	./run_script.bash 256 > output/my_reduced_results.txt
	./manager input/addresses2.txt input/BACKING_STORE.bin > output/my_writeback_results.txt

compare-orig:
	#Compare just the values (i.e., the rightmost field). Head removes the statistics from the end
	#of my results
	$(AWK) output/my_orig_results.txt | head -n -3 | diff - output/values_correct.txt

compare-reduced:
	$(AWK) output/my_reduced_results.txt | head -n -3 | diff - output/values_correct.txt

compare-writeback:
	$(AWK) output/my_writeback_results.txt | head -n -3 | diff - output/values_correct.txt

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
