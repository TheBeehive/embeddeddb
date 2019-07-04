#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "database.h"

#define PAGE_SIZE (getpagesize())

struct database_file_t
{
	size_t active_page;
};

static off_t get_page_offset(size_t number)
{
	return number * PAGE_SIZE;
}

static size_t get_page_number(off_t offset)
{
	return offset / PAGE_SIZE;
}

static transaction_t *start_read_transaction(database_t *database)
{
	transaction_t *transaction = malloc(sizeof(transaction_t));
	transaction->page = database->file->active_page;
	transaction->data = (char *) database->file + get_page_offset(database->file->active_page);
	return transaction;
}

static transaction_t *start_read_write_transaction(database_t *database)
{
	transaction_t *transaction = malloc(sizeof(transaction_t));
	struct stat st;
	int r = fstat(database->fd, &st);
	ftruncate(database->fd, st.st_size + PAGE_SIZE);
	transaction->page = get_page_number(st.st_size);
	transaction->data = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, st.st_size);
	// TODO: should I instead keep updating database->file to point to the active_page
	// instead of remapping active_page from the file to a valid place in memory when
	// I need it?
	char *active_page = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, get_page_offset(database->file->active_page));
	memcpy(transaction->data, active_page, PAGE_SIZE);
	r = munmap(active_page, PAGE_SIZE);
	return transaction;
}

static transaction_t *start_write_transaction(database_t *database)
{
	struct stat st;
	int r = fstat(database->fd, &st);
	ftruncate(database->fd, st.st_size + PAGE_SIZE);
	transaction_t *transaction = malloc(sizeof(transaction_t));
	transaction->page = get_page_number(st.st_size);
	transaction->data = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, st.st_size);
	return transaction;
}

database_t *database_new()
{
	if (PAGE_SIZE < sizeof(database_file_t))
		exit(1);

	database_t *database = malloc(sizeof(database_t));

    database->fd = open("/tmp/example", O_RDWR | O_CREAT, 0666);

    int r = ftruncate(database->fd, PAGE_SIZE * 2);

	database->file = mmap(NULL, PAGE_SIZE * 2, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, 0);

	database->file->active_page = 1;

	memset(((char *) database->file) + PAGE_SIZE, 0, PAGE_SIZE);

	return database;
}

void database_close(database_t *database)
{
	int r = munmap(database, PAGE_SIZE);
    printf("munmap(buf1): %i\n", r);
}

transaction_t *start_transaction(database_t *database, TRANSACTION_MODE tm)
{
	switch (tm)
	{
		case TRANSACTION_MODE_READ:
			return start_read_transaction(database);
		case TRANSACTION_MODE_WRITE:
			return start_write_transaction(database);
		case TRANSACTION_MODE_RW:
			return start_read_write_transaction(database);
		default:
			exit(1);
	}
}

void commit_transaction(database_t *database, transaction_t *transaction)
{
	database->file->active_page = transaction->page;
	free(transaction);
}

void cancel_transaction(database_t *database, transaction_t *transaction) 
{
	free(transaction);
}
