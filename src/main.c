#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define MAX_OFFSET        MAX_PAGE_NUMBER
#define PAGE_BYTES        256
#define NUMBER_FRAMES     256
#define FRAME_BYTES       PAGE_BYTES

typedef uint16_t virtual_address_t;

/**
  * Used to hold the number of a page, in the range 0 to 255
  */
typedef uint8_t page_number_t;

/**
  * Used to hold the offset from the beginning of a page/frame that a data value begins at, in the
  * rane 0 to 255
  */
typedef uint8_t offset_t;

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


typedef struct
{
	size_t next_frame;
	frameval_t table[NUMBER_FRAMES][FRAME_BYTES];
} frame_table_t;

typedef struct
{
	frame_number_t frame;
	uint8_t valid;
} page_entry_t;

typedef struct
{
	page_entry_t table[NUMBER_PAGES];
} page_table_t;

status_t perform_management(FILE *fin, FILE *backing);
status_t convert(char *line, size_t length, virtual_address_t *value);
virtual_components_t get_components(virtual_address_t address);
page_number_t get_page(virtual_address_t address);
offset_t get_offset(virtual_address_t address);
status_t load_if_necessary(page_table_t *ptable, page_number_t page, frame_table_t *ftable, FILE *backing);
physical_address_t get_physical_address(page_table_t *ptable, virtual_components_t *components);
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
	size_t translated = 0;
	frame_table_t frames = {0};
	page_table_t page_table = {0};
	
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
		else
		{
			//get the page and offset from the address
			virtual_components_t components = get_components(address);

			if ((error = load_if_necessary(&page_table, components.page, &frames, backing)) != SUCCESS)
			{
				free(line);
				return error_message(error);
			}
			else
			{
				translated++;
				physical_address_t phys_addr = get_physical_address(&page_table, &components);
				frameval_t memval = get_value_at_address(&frames, phys_addr);
				fprintf(stdout, "Virtual address: %u Physical address: %u Value: %d\r\n", address, phys_addr, memval);
			}
		}
	}

	fprintf(stdout, "Number of Translated Addresses = %zu", translated);

	free(line);
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

status_t load_if_necessary(page_table_t *ptable, page_number_t page, frame_table_t *frames, FILE *backing)
{
	if (!ptable->table[page].valid)
	{		
		//adjust the backing store file to the correct position
		if (fseek(backing, page * FRAME_BYTES, SEEK_SET) < 0)
		{
			return SEEK_ERROR;
		}

		//read the file into the frames table at the next available frame
		if (fread(frames->table + frames->next_frame, FRAME_BYTES, 1, backing) < 1) 
		{
			return READ_ERROR;
		}

		//then indicate the frame associated with the page and mark the table entry valid
		ptable->table[page].frame = frames->next_frame;
		ptable->table[page].valid = 1;
		//then update what the next frame will be
		frames->next_frame++;
	}

	return SUCCESS;
}

physical_address_t get_physical_address(page_table_t *ptable, virtual_components_t *components)
{
	return ptable->table[components->page].frame * FRAME_BYTES + components->offset;
}

frameval_t get_value_at_address(frame_table_t *frames, physical_address_t phys_addr)
{
	return *((frameval_t *) frames->table + phys_addr); 
}

status_t error_message(status_t error)
{
	switch (error)
	{
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
