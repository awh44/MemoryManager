# MemoryManager
A simulation of a virtual memory manager for an operating systems course.

## Source
The source files contained herein are as follows:

	src/main.c - the main driver file
	src/lru_queue.c - an implementation of a LRU queue data type
	include/lru_queue.h - the header file for the LRU queue

## Output
Also included are output results from a frame table of size 256, a frame table
of size 128, and a frame table of size 128 with a dirty bit indication, at the
following locations, respectively:

	output/my_orig_results.txt
	output/my_reduced_results.txt
	output/my_writeback_results.txt
	
Also included in this directory is the original correct.txt file as well as a
file which contains only the rightmost value field, values\_correct.txt. Note
that correct.txt has been converted from DOS ("\r\n") line endings to UNIX
("\n") ones and that a newline has been added at the end of the file.

## Compilation and Output Generation
Two files for compiling/creating/running are included:

	makefile
	run_script.bash
	
The makefile supports the following targets:

	view-results - view the three above-indicated files in the output folder in
	               a pager. (Does not force these files to update if there have
	               been changes to source files.)
	make-results - this will re-run all of the different things needed to
	               recreate the three above-indicated output files. Makes use
	               of the run_script.bash file, which recompiles manager as
	               needed to get the different functionality for the differing
                   frame table sizes. Only run this target if you'd like to
                   recreate the three output files.
	compare-orig - this will perform a comparison between my_orig_results.txt
	               and correct.txt
	compare-reduced - this will perform a comparison between the value fields of
	                  my_reduced_results.txt and of values_correct.txt
	compare-writeback - the same as above but with my_writeback_results.txt
	FILE=x BACKING=y make run - compiles and runs program with input file x and
	                            backing store y
	manager - compiles the main manager program
	build/main.o - compiles the main driver program
	build/lru_queue.o - compiles the lru_queue data type
	clean - removes manager and build files

## Input
Also included are the given input files:

	input/addresses.txt
	input/addresses2.txt
	input/BACKING_STORE.bin
	
Note that the addresses have been sanitized to be of UNIX line endings ("\n")
instead of DOS ("\r\n") ones. If it is not the case that the input files have
this property, then the line #define EXTRA\_CHARS 1 in src/main.c must be
changed to #define EXTRA\_CHARS 2
