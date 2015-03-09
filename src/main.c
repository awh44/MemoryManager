#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/lru_queue.h"

/**
  * After a read of a line, determines whether the line-endings are UNIX or Windows style,
  * 'returning' the number of extraneous characters
  */
#define EXTRA_CHARS(line, chars_read) line[chars_read - 2] == '\r' ? 2 : 1

#define MIN_ARGS 3
#define ASCII_0 48

/**
  * Define error constants. Contain a mixture of user (e.g., not enough command line arguments) and
  * system (e.g., couldn't read from an open file) errors
  */
#define SUCCESS    0
#define ARGS_ERROR 1
#define OPEN_ERROR 2
#define NUMB_ERROR 3
#define SEEK_ERROR 4
#define READ_ERROR 5

/**
  * Define the type for the erorrs. Allows for a total of 2^64 - 1 types of error conditions (plus a
  * succcess condition)
  */
typedef uint64_t status_t;


/**
  * Define variosu contants for the size of pages, the total number of pages, the size of frames,
  * the total number of frames, etc. Note that these constants assume that the number of bits in a
  * page number is the same as the number of bits in the offset.
  */
#define BITS_PER_BYTE     8
#define OFFSET_BITS       sizeof(offset_t) * BITS_PER_BYTE
#define NUMBER_PAGES      256 
#define MAX_PAGE_NUMBER   NUMBER_PAGES - 1
#define MAX_OFFSET        255 
#define PAGE_BYTES        256
#define NUMBER_FRAMES     128
#define MAX_FRAME_NUMBER  NUMBER_FRAMES - 1
#define FRAME_BYTES       PAGE_BYTES
#define TLB_ENTRIES       16
#define INVALID_PAGE      NUMBER_PAGES

typedef uint16_t virtual_address_t;

/**
  * Used to hold the number of a page, in the range 0 to MAX_PAGe_NUMBER (currently 255)
  */
typedef uint8_t page_number_t;

/**
  * Used to hold the offset from the beginning of a page/frame that a data value begins at, in the
  * range 0 to MAX_OFFSET (currently 255)
  */
typedef uint8_t offset_t;

/**
  * Holds the two components of a virtual address, the page number and the offset within the page
  */
typedef struct
{
	page_number_t page;
	offset_t offset;
} virtual_components_t;

/**
  * Used to hold a physical address value, in the range 0 to 65535
  */
typedef uint16_t physical_address_t;

/**
  * Used to hold a frame number, from 0 to NUMBER_FRAMES - 1
  */
typedef uint8_t frame_number_t;

/**
  * A single, individual value in a frame.
  */
typedef int8_t frameval_t;

/**
  * The frame table data structure. Holds the actual physical memory and information on which frame
  * is to be used next upon a page fault
  */
typedef struct
{
	frame_number_t used_frames;
	frameval_t table[NUMBER_FRAMES][FRAME_BYTES];
	page_number_t page_for_frame[NUMBER_FRAMES];
	lru_queue_t queue;
} frame_table_t;

/**
  * Used to represent a single entry in a page table, including which frame is being referenced and
  * whether the reference is currently valid
  */
typedef struct
{
	frame_number_t frame;
	uint8_t valid;
} page_entry_t;

/**
  * The data structure for the page table, made up of an array of page entries
  */
typedef struct
{
	page_entry_t table[NUMBER_PAGES];
} page_table_t;

/**
  * Holds the information on the TLB
  */
typedef struct
{
	uint16_t pages[TLB_ENTRIES];
	frame_number_t frames[TLB_ENTRIES];
	lru_queue_t queue;
} tlb_t;

typedef struct
{
	size_t translated;
	size_t page_faults;
	size_t tlb_hits;
} statistics_t;


static statistics_t statistics;

status_t perform_management(FILE *fin, FILE *backing);
status_t print_for_address(FILE *fin, FILE *backing, virtual_address_t address, frame_table_t *frames, page_table_t *page_table, tlb_t *tlb);

status_t convert(char *line, size_t length, virtual_address_t *value);
virtual_components_t get_components(virtual_address_t address);
offset_t get_offset(virtual_address_t address);
page_number_t get_page(virtual_address_t address);

void frame_table_initialize(frame_table_t *frames);
void frame_table_uninitialize(frame_table_t *frames);
status_t load_if_necessary(page_table_t *ptable, page_number_t page, frame_table_t *ftable, FILE *backing);
frame_number_t get_next_frame(frame_table_t *frames);

void tlb_initialize(tlb_t *tlb);
void tlb_uninitialize(tlb_t *tlb);
int get_frame_from_tlb(tlb_t *tlb, page_number_t page, frame_number_t *frame);

physical_address_t get_physical_address_from_page_table(page_table_t *ptable, virtual_components_t *components);
physical_address_t get_physical_address(frame_number_t frame, offset_t offset);
frameval_t get_value_at_address(frame_table_t *frames, physical_address_t phys_addr);


status_t error_message(status_t error);

int main(int argc, char *argv[])
{
	if (argc < MIN_ARGS)
	{
		return error_message(ARGS_ERROR);
	}
	
	FILE *fin;
	if ((fin = fopen(argv[1], "r")) == NULL)
	{
		return error_message(OPEN_ERROR);
	}

	FILE *backing;
	if ((backing = fopen(argv[2], "r")) == NULL)
	{
		fclose(fin);
		return error_message(OPEN_ERROR);
	}

	status_t error = perform_management(fin, backing);

	fclose(backing);
	fclose(fin);
	return error_message(error);
}

status_t perform_management(FILE *fin, FILE *backing)
{
	frame_table_t frames;
	frame_table_initialize(&frames);
	page_table_t page_table = {0};
	tlb_t tlb;
	tlb_initialize(&tlb);
	
	char *line = NULL;
	size_t size = 0;
	ssize_t chars_read;
	while ((chars_read = getline(&line, &size, fin)) > 0)
	{
		//eliminate the newline and (potentially) the carriage return
		chars_read -= EXTRA_CHARS(line, chars_read);
		line[chars_read] = '\0';
		
		//convert the string to the address
		virtual_address_t address;
		status_t error;
		if ((error = convert(line, chars_read, &address)) != SUCCESS)
		{
			error_message(error);
		}
		else if ((error = print_for_address(fin, backing, address, &frames, &page_table, &tlb)) != SUCCESS)
		{
			free(line);
			return error;
		}
	}

	fprintf(stdout, "Number of Translated Addresses = %zu\n", statistics.translated);
	fprintf(stdout, "Percentage of Page Faults = %lf\n", (double) statistics.page_faults / statistics.translated);
	fprintf(stdout, "TLB hit ratio = %lf\n", (double) statistics.tlb_hits / statistics.translated);

	free(line);
	tlb_uninitialize(&tlb);
	frame_table_uninitialize(&frames);
	return SUCCESS;
}

void frame_table_initialize(frame_table_t *frames)
{
	frames->used_frames = 0;
	lru_queue_initialize(&frames->queue);
	size_t i;
	for (i = 0; i < NUMBER_FRAMES; i++)
	{
		lru_queue_insert_new(&frames->queue, NUMBER_FRAMES - i - 1);
	}
}

void frame_table_uninitialize(frame_table_t *frames)
{
	lru_queue_uninitialize(&frames->queue);
}

void tlb_initialize(tlb_t *tlb)
{
	lru_queue_initialize(&tlb->queue);
	
	size_t i;
	for (i = 0; i < TLB_ENTRIES; i++)
	{
		tlb->pages[i] = INVALID_PAGE;
		lru_queue_insert_new(&tlb->queue, TLB_ENTRIES - i - 1);
	}
}

void tlb_uninitialize(tlb_t *tlb)
{
	lru_queue_uninitialize(&tlb->queue);
}

status_t convert(char *s, size_t length, virtual_address_t *value)
{
    *value = 0;
    size_t power10;
    size_t i;
    for (i = length - 1, power10 = 1; i < SIZE_MAX; i--, power10 *= 10)
    {
        if (!isdigit(s[i]))
        {
            return NUMB_ERROR;
        }
        *value += (s[i] - ASCII_0) * power10;
    }

    return SUCCESS;
}

status_t print_for_address(FILE *fin, FILE *backing, virtual_address_t address, frame_table_t *frames, page_table_t *page_table, tlb_t *tlb)
{
	//get the page and offset from the address
	virtual_components_t components = get_components(address);

	frame_number_t frame;
	physical_address_t phys_addr;
	int tlb_entry;
	if ((tlb_entry = get_frame_from_tlb(tlb, components.page, &frame)) != TLB_ENTRIES)
	{
		phys_addr = get_physical_address(frame, components.offset);
	}
	else 
	{
		status_t error;
		if ((error = load_if_necessary(page_table, components.page, frames, backing)) != SUCCESS)
		{
			return error;
		}

		phys_addr = get_physical_address_from_page_table(page_table, &components);

		//update the tlb
		tlb_entry = lru_queue_get(&tlb->queue);
		tlb->pages[tlb_entry] = components.page;
		tlb->frames[tlb_entry] = page_table->table[components.page].frame;
	}


	//update the lru information for the tlb
	lru_queue_update_existing(&tlb->queue, tlb_entry);
	statistics.translated++;
	frameval_t memval = get_value_at_address(frames, phys_addr);
	fprintf(stdout, "Virtual address: %u Physical address: %u Value: %d\r\n", address, phys_addr, memval);
	return SUCCESS;
}

virtual_components_t get_components(virtual_address_t address)
{
	virtual_components_t components = { get_page(address), get_offset(address) };
	return components;
}

page_number_t get_page(virtual_address_t address)
{
	//shift right by the OFFSET_SIZE (because the offset takes up the lower bits), and than AND by
	//the max value to get the value of the upper bits are
	return (address >> OFFSET_BITS) & MAX_PAGE_NUMBER;
}

offset_t get_offset(virtual_address_t address)
{
	return address & MAX_OFFSET;
}

int get_frame_from_tlb(tlb_t *tlb, page_number_t page, frame_number_t *frame)
{
	size_t i;
	for (i = 0; i < TLB_ENTRIES; i++)
	{
		if (tlb->pages[i] == page)
		{
			statistics.tlb_hits++;
			*frame = tlb->frames[i];
			return i;
		}
	}

	return TLB_ENTRIES;
}

status_t load_if_necessary(page_table_t *ptable, page_number_t page, frame_table_t *frames, FILE *backing)
{
	if (!ptable->table[page].valid)
	{
		statistics.page_faults++;
		//adjust the backing store file to the correct position
		if (fseek(backing, page * FRAME_BYTES, SEEK_SET) < 0)
		{
			return SEEK_ERROR;
		}

		frame_number_t next_frame;
		if (frames->used_frames < NUMBER_FRAMES)
		{
			next_frame = frames->used_frames;
			frames->used_frames++;
		}
		else
		{
			next_frame = lru_queue_get(&frames->queue);
			//invalid the page previously at the frame
			ptable->table[frames->page_for_frame[next_frame]].valid = 0;
		}
		//read the file into the frames table at the next available frame
		if (fread(frames->table + next_frame, FRAME_BYTES, 1, backing) < 1) 
		{
			return READ_ERROR;
		}

		//then indicate the frame associated with the page and mark the table entry valid
		ptable->table[page].frame = next_frame;
		ptable->table[page].valid = 1;
		//and associate the given frame with the new page
		frames->page_for_frame[next_frame] = page;

	}

	//indicate that the frame has just been referenced to bring it to the front of the LRU queue
	lru_queue_update_existing(&frames->queue, ptable->table[page].frame);

	return SUCCESS;
}

physical_address_t get_physical_address_from_page_table(page_table_t *ptable, virtual_components_t *components)
{
	return get_physical_address(ptable->table[components->page].frame, components->offset);
}

physical_address_t get_physical_address(frame_number_t frame, offset_t offset)
{
	return frame * FRAME_BYTES + offset;
}

frameval_t get_value_at_address(frame_table_t *frames, physical_address_t phys_addr)
{
	return *((frameval_t *) frames->table + phys_addr); 
}

status_t error_message(status_t error)
{
	switch (error)
	{
		case SUCCESS:
			break;
		case ARGS_ERROR:
			fprintf(stderr, "Error: please include input files as command line arguments.\n");
			break;
		case OPEN_ERROR:
			fprintf(stderr, "Error: could not open file.\n");
			break;
		case NUMB_ERROR:
			fprintf(stderr, "Error: could not convert string to integer.\n");
			break;
		case SEEK_ERROR:
			fprintf(stderr, "Error: could not seek in file.\n");
			break;
		case READ_ERROR:
			fprintf(stderr, "Error: could not read from file.\n");
			break;
		default:
			fprintf(stderr, "Error: unknown error.\n");
			break;
	}

	return error;
}
