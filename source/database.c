#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "database.h"

#define PAGE_SIZE (getpagesize())
#define NUM_VERSIONS_INIT 100


// TODO: improve error handling instead of just exit(1) and do cleanup
// TODO: thread-safety -- make some stuff single-threaded; also, add atomics and
// mutexes to stuff that is potentially multi-threaded
// TODO: make refcount array a vector
// TODO: only ftruncate periodically and synchronize refcount array
// TODO: mmap less (it is slow)

// TODO: if going more than 1page, use binary tree

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
	transaction->read_page = database->file->active_page;

	database->refcount[transaction->read_page] += 1;
	transaction->data = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, get_page_offset(transaction->read_page));

	return transaction;
}

static void commit_read_transaction(database_t *database, transaction_t *transaction)
{
	if (transaction->read_page > database->num_versions)
		exit(1);
	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

static void cancel_read_transaction(database_t *database, transaction_t *transaction)
{
	if (transaction->read_page > database->num_versions)
		exit(1);
	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

static transaction_t *start_write_transaction(database_t *database)
{
	struct stat st;
	int r = fstat(database->fd, &st);

	transaction_t *transaction = malloc(sizeof(transaction_t));
	transaction->write_page = SIZE_MAX;
	for (size_t i = 0; i < database->num_versions; i++)
	{
		if (database->refcount[i] == 0)
		{
			transaction->write_page = i;
			break;
		}
	}

	if (transaction->write_page == SIZE_MAX)
	{
		/* no available pages. resize the file to have a new page */
		ftruncate(database->fd, st.st_size + PAGE_SIZE);
		transaction->write_page = get_page_number(st.st_size);
		/* if page number is > 100, will need to resize refcount array */
		if (transaction->write_page > database->num_versions)
		{
			database->num_versions += NUM_VERSIONS_INIT;
			database->refcount = realloc(database->refcount, database->num_versions);
		}
	}
	
	off_t offset = get_page_offset(transaction->write_page);

	transaction->data = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, offset);

	return transaction;
}

static void commit_write_transaction(database_t *database, transaction_t *transaction)
{
	database->file->active_page = transaction->write_page;
	free(transaction);
}

static void cancel_write_transaction(database_t *database, transaction_t *transaction)
{
	free(transaction);
}

static transaction_t *start_read_write_transaction(database_t *database)
{
	// XXX: should I instead keep updating database->file to point to the active_page
	// instead of remapping active_page from the file to a valid place in memory when
	// I need it?
	size_t active_page_num = database->file->active_page;
	off_t active_page_offset = get_page_offset(active_page_num);
	char *active_page_addr = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, active_page_offset);
	database->refcount[active_page_num] += 1;

	transaction_t *transaction = start_write_transaction(database);
	memcpy((char *)transaction->data, active_page_addr, PAGE_SIZE);
	transaction->read_page = active_page_num;
	int r = munmap(active_page_addr, PAGE_SIZE);
	return transaction;
}

static void commit_read_write_transaction(database_t *database, transaction_t *transaction)
{
	database->file->active_page = transaction->write_page;

	if (transaction->read_page > database->num_versions)
		exit(1);
	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

static void cancel_read_write_transaction(database_t *database, transaction_t *transaction)
{
	if (transaction->read_page > database->num_versions)
		exit(1);
	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

database_t *database_new(char *filename)
{
	if (PAGE_SIZE < sizeof(database_file_t))
		exit(1);

	database_t *database = malloc(sizeof(database_t));
	database->num_versions = NUM_VERSIONS_INIT;
	database->refcount = malloc(sizeof(int) * database->num_versions);
	for (size_t i = 0; i < database->num_versions; i++)
	{
		database->refcount[i] = 0;
	}

	int fd = open(filename, O_RDWR | O_CREAT, 0666);
	if (fd < 0)
		exit(1);
	database->fd = fd;

    int r = ftruncate(database->fd, PAGE_SIZE * 2);
	if (r == -1)
		exit(1);

	database->file = mmap(NULL, PAGE_SIZE * 2, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, 0);
	if (database->file == MAP_FAILED)
		exit(1);

	database->file->active_page = 1;

	memset(((char *) database->file) + PAGE_SIZE, 0, PAGE_SIZE);

	return database;
}

void database_close(database_t *database)
{
	int r = munmap(database, PAGE_SIZE);
    printf("munmap(buf1): %i\n", r);
	// XXX: do I need to explicitly free each member of the refcount array?
	free(database);
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
	switch (transaction->tm)
	{
		case TRANSACTION_MODE_READ:
			return commit_read_transaction(database, transaction);
		case TRANSACTION_MODE_WRITE:
			return commit_write_transaction(database, transaction);
		case TRANSACTION_MODE_RW:
			return commit_read_write_transaction(database, transaction);
		default:
			exit(1);
	}
}

void cancel_transaction(database_t *database, transaction_t *transaction) 
{
	switch (transaction->tm)
	{
		case TRANSACTION_MODE_READ:
			return cancel_read_transaction(database, transaction);
		case TRANSACTION_MODE_WRITE:
			return cancel_write_transaction(database, transaction);
		case TRANSACTION_MODE_RW:
			return cancel_read_write_transaction(database, transaction);
		default:
			exit(1);
	}
}
