#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUCCESS    0
#define ARGS_ERROR 1
#define OPEN_ERROR 2
#define NUMB_ERROR 3

#define MIN_ARGS 3
#define ASCII_0 48

#define BITS_PER_BYTE     8
#define ADDRESS_BITS      sizeof(address_t) * BITS_PER_BYTE
#define PAGE_NUMBER_BITS  sizeof(page_address_t) * BITS_PER_BYTE
#define OFFSET_BITS       sizeof(offset_t) * BITS_PER_BYTE
#define MAX_PAGE_NUMBER   UINT8_MAX 
#define NUMBER_PAGES      MAX_PAGE_NUMBER + 1
#define MAX_OFFSET        UINT8_MAX 
#define PAGE_BYTES        256
#define NUMBER_FRAMES     256
#define FRAME_BYTES       PAGE_BYTES

#define EXTRA_CHARS(line, chars_read) line[chars_read - 2] == '\r' ? 2 : 1

typedef uint16_t address_t;
typedef uint8_t page_number_t;
typedef uint8_t offset_t;
typedef uint8_t frame_number_t;
typedef int8_t frameval_t;
typedef frameval_t frame_t[FRAME_BYTES];
typedef struct
{
	frame_number_t frame;
	uint8_t valid;
} page_entry_t;

typedef uint64_t status_t;

status_t convert(char *line, size_t length, address_t *value);
void get_page_and_offset(address_t address, page_number_t *page, offset_t *offset);
page_number_t get_page(address_t address);
offset_t get_offset(address_t address);
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
		return error_message(OPEN_ERROR);
	}

	frame_t frames[NUMBER_FRAMES] = {0};
	page_entry_t page_table[NUMBER_PAGES] = {0};
	char *line = NULL;
	size_t size = 0;
	ssize_t chars_read;
	while ((chars_read = getline(&line, &size, fin)) > 0)
	{
		//eliminate the newline and (potentially) the carriage return
		chars_read -= EXTRA_CHARS(line, chars_read);
		line[chars_read] = '\0';
		
		//convert the string to the address
		address_t address;
		status_t error;
		if ((error = convert(line, chars_read, &address)) != SUCCESS)
		{
			error_message(error);
		}
		else
		{
			//get the page and offset from the address
			page_number_t page;
			offset_t offset;
			get_page_and_offset(address, &page, &offset);
	
			int8_t memval;
			fseek(backing, page * PAGE_BYTES + offset, SEEK_SET);
			fread(&memval, sizeof(memval), 1, backing);

			//print everything out
			fprintf(stdout, "%s, %u, %u, %u, %d\n", line, address, page, offset, memval);
		}
	}

	free(line);
	fclose(backing);
	fclose(fin);

	return 0;
}

status_t convert(char *s, size_t length, address_t *value)
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

void get_page_and_offset(address_t address, page_number_t *page, offset_t *offset)
{
	*page = get_page(address);
	*offset = get_offset(address);
}

page_number_t get_page(address_t address)
{
	//shift right by the OFFSET_SIZE (because the offset takes up the lower bits), and than AND by
	//the max value to get the value of the upper bits are
	return (address >> OFFSET_BITS) & MAX_PAGE_NUMBER;
}

offset_t get_offset(address_t address)
{
	return address & MAX_OFFSET;
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
		default:
			fprintf(stderr, "Error: unknown error.\n");
			break;
	}

	return error;
}
