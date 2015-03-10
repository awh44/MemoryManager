#!/bin/bash
if (($# < 1)); then
	exit
fi

NUMBER=$(grep "#define NUMBER_FRAMES     $1" src/main.c | wc -l)
if (($NUMBER > 0)); then
	NEW=256
	if (($1 == 256)); then
		NEW=128
	fi
	$(sed -i -- "s/#define NUMBER_FRAMES     $1/#define NUMBER_FRAMES     $NEW/" src/main.c)
	$(gcc -c -obuild/main.o src/main.c)
	$(gcc -omanager build/main.o build/lru_queue.o)
fi
./manager input/addresses.txt input/BACKING_STORE.bin
