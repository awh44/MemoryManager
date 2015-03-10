#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/lru_queue.h"

#define EXTRA_CHARS 2

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
  * Used to hold a physical address value, in the range 0 to 32768 (NUMBER_FRAMES * FRAME_BYTES)
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
	uint8_t dirty;
} page_entry_t;

/**
  * The data structure for the page table, made up of an array of page entries
  */
typedef struct
{
	page_entry_t table[NUMBER_PAGES];
} page_table_t;

/**
  * Holds the information on the TLB, including which page numbers are in it and which entry is to
  * be replaced next
  */
typedef struct
{
	uint16_t pages[TLB_ENTRIES];
	frame_number_t frames[TLB_ENTRIES];
	lru_queue_t queue;
} tlb_t;

/**
  * Holds a variety of statistical information, including total number of addresses translated,
  * number of page faults, number of tlb hits, and number of write-backs occuring
  */
typedef struct
{
	size_t translated;
	size_t page_faults;
	size_t tlb_hits;
	size_t write_backs;
} statistics_t;

static statistics_t statistics;

//DRIVER FUNCTIONS---------------------------------------------------------------------------------
	/**
	  * After initialization, acts as the main driving function
	  * @param fin     the file from which the memory addresses will be read
	  * @param backing the file which contains the backing store
	  * @return an indication of whether an error occurred
	  */
	status_t perform_management(FILE *fin, FILE *backing);

	/**
	  * For a single virtual address, this function will perform all necessary calculations and
	  * retrieves to ultimately print out the value at the address
	  * @param fin        the file from which the memory addresses are read
	  * @param backing    the file which contains the backing store
	  * @param address    the virtual address being accesses
	  * @param frames     the frame table
	  * @param page_table the page_table
	  * @param tlb        the current tlb
	  * @param is_write   whether the current memory access is a write or not
	  * @return an indication of whether an error occurred
	  */
	status_t print_for_address(FILE *fin, FILE *backing, virtual_address_t address, frame_table_t *frames, page_table_t *page_table, tlb_t *tlb, uint8_t is_write);
//END DRIVER FUNCTIONS------------------------------------------------------------------------------

//VIRTUAL ADDRESS FUNCTIONS-------------------------------------------------------------------------
	/**
	  * Given a string of a particular length, converts it to a virtual address value
	  * @param line   the string to be converted
	  * @param length the length of the string
	  * @param value  the out parameter which will hold the calculated virtual address
	  * @return an indication of whether an error occurred
	  */
	status_t convert(char *line, size_t length, virtual_address_t *value);

	/**
	  * Pulls out the components, the page and the offset, of the virtual address
	  * @param address the virtual address of which to get the components
	  * @return a structure containing the components of the address
	  */
	virtual_components_t get_components(virtual_address_t address);

	/**
	  * Pulls out the offset of the virtual address
	  * @param address the virtual address of which to get the offset
	  * @return the offset of the address
	  */
	offset_t get_offset(virtual_address_t address);

	/**
	  * Pulls out the page number of the virtual address
	  * @param address the virtual address of which to get the page number
	  * @return the page number of the address
	  */
	page_number_t get_page(virtual_address_t address);
//END VIRTUAL ADDRESS FUNCTIONS---------------------------------------------------------------------

//FRAME TABLE FUNCTIONS-----------------------------------------------------------------------------
	/**
	  * Initializes a frame table data structure after it has been declared
	  * @param frames the frame table to initialize
	  */
	void frame_table_initialize(frame_table_t *frames);

	/**
	  * Uninitializes a frame table data structure after it is no longer needed.
	  * @param frames the frame table to uninitialize
	  */
	void frame_table_uninitialize(frame_table_t *frames);

	/**
	  * If the page at the given page number is not already loaded into a frame, this function will
	  * load it into a frame from the backing store, updating the page table as necessary; if the
	  * page is already loaded, this function has no effect
	  * @param ptable  the current page table
	  * @param page    the number of the page that needs to be loaded
	  * @param ftable  the current frame table
	  * @param backing the backing store holding all memory information
	  * @return an indication of whether an error occurred
	  */
	status_t load_if_necessary(page_table_t *ptable, page_number_t page, frame_table_t *ftable, FILE *backing);

	/**
	  * Gets the value from the frame table at the particular address
	  * @param frames    the frame table
	  * @param phys_addr the physical address from which the value is being retrieved
	  * @return the value at the given physical address
	  */
	frameval_t get_value_at_address(frame_table_t *frames, physical_address_t phys_addr);
//END FRAME TABLE FUNCTIONS------------------------------------------------------------------------

//TLB FUNCTIONS------------------------------------------------------------------------------------
	/**
	  * Initializes a TLB data structure after it has been declared
	  * @param tlb the TLB to be initialized
	  */
	void tlb_initialize(tlb_t *tlb);

	/**
	  * Uninitializes a TLB data structure after it is no longer needed
	  * @param tlb the TLB to be uninitialized
	  */
	void tlb_uninitialize(tlb_t *tlb);

	/**
	  * Tries to get the frame for the given page from the given TLB. Places the frame in frame and
	  * returns the index in the TLB of the discovered entry, upon success. Upon failure, returns
	  * TLB_ENTRIES
	  * @param tlb   the tlb to be searched
	  * @param page  the number of the page for which the frame is to be found
	  * @param frame out param which will hold the frame number for the page upon success
	  * @return if the frame is successfully found, returns the index in the tlb of the tlb entry;
	  * otherwise, returns TLB_ENTRIES
	  */
	int get_frame_from_tlb(tlb_t *tlb, page_number_t page, frame_number_t *frame);
//END TLB FUNCTIONS--------------------------------------------------------------------------------

//PHYSICAL ADDRESS FUNCTIONS-----------------------------------------------------------------------
	/**
	  * Given a page table and the components of a virtual address, returns the physical address
	  * corresponding to the components
	  * @param ptable     the current page table
	  * @param components the components of the virtual address
	  * @return the physical address corresponding to the components
	  */
	physical_address_t get_physical_address_from_page_table(page_table_t *ptable, virtual_components_t *components);

	/**
	  * Given a frame number and an offset, returns the corresponding physical address
	  * @param frame  the frame number
	  * @param offset the offset from the beginning of the frame
	  * @return the physical address corresponding to the frame and the offset
	  */
	physical_address_t get_physical_address(frame_number_t frame, offset_t offset);
//PHYSICAL ADDRESS FUNCTIONS-----------------------------------------------------------------------

//ERROR FUNCTIONS
	/**
	  * Prints an error message for an error of type status_t, returning the passed error upon
	  * completion
	  * @param error the error
	  * @return the passed error
	  */
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
		//eliminate the newline and the carriage return as well as one for the space
		//and the Read/Write indicator
		chars_read -= EXTRA_CHARS;

		//sanitize input - it cannot be assumed that the input file has a regular form - make sure
		//to handle R/W indications and no R/W indications as well as the occasional space at the
		//end of a line
		if (line[chars_read - 1] == ' ')
		{
			chars_read--;
		}
		uint8_t is_write = line[chars_read - 1] == 'W';
		if (is_write || line[chars_read - 1] == 'R')
		{
			chars_read -= 2;
		}
		line[chars_read] = '\0';
		
		//convert the string to the address
		virtual_address_t address;
		status_t error;
		if ((error = convert(line, chars_read, &address)) != SUCCESS)
		{
			fprintf(stderr, "%s\n", line);
			error_message(error);
		}
		else if ((error = print_for_address(fin, backing, address, &frames, &page_table, &tlb, is_write)) != SUCCESS)
		{
			free(line);
			return error;
		}
	}

	fprintf(stdout, "Number of Translated Addresses = %zu\n", statistics.translated);
	fprintf(stdout, "Percentage of Page Faults = %lf (absolute = %zu)\n", (double) statistics.page_faults / statistics.translated, statistics.page_faults);
	fprintf(stdout, "TLB Hit Ratio = %lf (absolute = %zu)\n", (double) statistics.tlb_hits / statistics.translated, statistics.tlb_hits);
	fprintf(stdout, "Write-Backs = %zu\n", statistics.write_backs);

	free(line);
	tlb_uninitialize(&tlb);
	frame_table_uninitialize(&frames);
	return SUCCESS;
}

status_t print_for_address(FILE *fin, FILE *backing, virtual_address_t address, frame_table_t *frames, page_table_t *page_table, tlb_t *tlb, uint8_t is_write)
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

	//actually retrieve the memory value at the given physical address
	frameval_t memval = get_value_at_address(frames, phys_addr);
	fprintf(stdout, "Virtual address: %u Physical address: %u Value: %d\n", address, phys_addr, memval);

	//if it's a write, set the dirty bit after the memory access
	if (is_write)
	{
		page_table->table[components.page].dirty = 1;
	}

	//update the LRU information for the tlb
	lru_queue_update_existing(&tlb->queue, tlb_entry);

	
	//indicate that the frame has just been referenced to bring it to the front of the LRU queue
	lru_queue_update_existing(&frames->queue, page_table->table[components.page].frame);

	//update the statistics
	statistics.translated++;

	return SUCCESS;
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
			//invalidate the page previously at the frame. Note that the TLB does not have to be
			//updated because both the TLB and the frame table use LRU behavior to determine the
			//next thing to remove. Therefore, if something is in the TLB, there is no way it could
			//be invalidated here, because it would have been used more recently than 128 pages ago
			//(because the TLB has only 16 entries)
			page_number_t prev_page = frames->page_for_frame[next_frame];
			ptable->table[prev_page].valid = 0;
			if (ptable->table[prev_page].dirty)
			{
				statistics.write_backs++;
			}
		}
		//read the file into the frames table at the next available frame
		if (fread(frames->table + next_frame, FRAME_BYTES, 1, backing) < 1) 
		{
			return READ_ERROR;
		}

		//then indicate the frame associated with the page and mark the table entry valid and
		//undirty
		ptable->table[page].frame = next_frame; 
		ptable->table[page].valid = 1;
		ptable->table[page].dirty = 0;
		//and associate the given frame with the new page
		frames->page_for_frame[next_frame] = page;

	}

	return SUCCESS;
}

frameval_t get_value_at_address(frame_table_t *frames, physical_address_t phys_addr)
{
	return *((frameval_t *) frames->table + phys_addr); 
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

physical_address_t get_physical_address_from_page_table(page_table_t *ptable, virtual_components_t *components)
{
	return get_physical_address(ptable->table[components->page].frame, components->offset);
}

physical_address_t get_physical_address(frame_number_t frame, offset_t offset)
{
	return frame * FRAME_BYTES + offset;
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
