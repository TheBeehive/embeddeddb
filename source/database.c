#include <assert.h>
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


// TODO: thread-safety -- make some stuff single-threaded; also, add atomics and
// mutexes to stuff that is potentially multi-threaded
// TODO: make refcount array a vector
// TODO: only ftruncate periodically and synchronize refcount array
// TODO: mmap less (it is slow). how could I avoid mmapping for a read
// transaction?

// TODO: check at database creation time if (PAGE_SIZE < sizeof(database_file_t))
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
	transaction_t *transaction;
	if ((transaction = malloc(sizeof(transaction_t))) == NULL)
		return NULL;
	transaction->read_page = database->file->active_page;

	database->refcount[transaction->read_page] += 1;

	if ((transaction->data = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, get_page_offset(transaction->read_page))) == MAP_FAILED)
		return NULL;

	return transaction;
}

static void commit_read_transaction(database_t *database, transaction_t *transaction)
{
	assert(transaction->read_page <= database->num_versions);

	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

static void cancel_read_transaction(database_t *database, transaction_t *transaction)
{
	assert(transaction->read_page <= database->num_versions);

	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

static transaction_t *start_write_transaction(database_t *database)
{
	struct stat st;
	int r;
	if ((r = fstat(database->fd, &st)) == -1)
		return NULL;

	transaction_t *transaction;
	if ((transaction = malloc(sizeof(transaction_t))) == NULL)
		return NULL;
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
		if ((ftruncate(database->fd, st.st_size + PAGE_SIZE)) == -1)
			return NULL;
		transaction->write_page = get_page_number(st.st_size);
		/* if page number is > 100, will need to resize refcount array */
		if (transaction->write_page > database->num_versions)
		{
			database->num_versions += NUM_VERSIONS_INIT;
			if ((database->refcount = realloc(database->refcount, database->num_versions)) == NULL)
				return NULL;
		}
	}
	
	off_t offset = get_page_offset(transaction->write_page);

	if ((transaction->data = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, offset)) == MAP_FAILED)
		return NULL;

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
	char *active_page_addr;
	if ((active_page_addr = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, active_page_offset)) == MAP_FAILED)
		return NULL;
	database->refcount[active_page_num] += 1;

	transaction_t *transaction = start_write_transaction(database);
	memcpy((char *)transaction->data, active_page_addr, PAGE_SIZE);
	transaction->read_page = active_page_num;
	int r;
	if ((r = munmap(active_page_addr, PAGE_SIZE)) == -1)
		return NULL;
	return transaction;
}

static void commit_read_write_transaction(database_t *database, transaction_t *transaction)
{
	database->file->active_page = transaction->write_page;

	assert(transaction->read_page <= database->num_versions);
	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

static void cancel_read_write_transaction(database_t *database, transaction_t *transaction)
{
	assert(transaction->read_page <= database->num_versions);

	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

database_t *database_new(char *filename)
{
	database_t *database;
	if ((database = malloc(sizeof(database_t))) == NULL)
		return NULL;
	database->num_versions = NUM_VERSIONS_INIT;
	if ((database->refcount = malloc(sizeof(int) * database->num_versions)) == NULL)
		return NULL;
	for (size_t i = 0; i < database->num_versions; i++)
	{
		database->refcount[i] = 0;
	}

	if ((database->fd = open(filename, O_RDWR | O_CREAT, 0666)) == -1)
		return NULL;

    int r;
	if ((r = ftruncate(database->fd, PAGE_SIZE * 2)) == -1)
		return NULL;

	if ((database->file = mmap(NULL, PAGE_SIZE * 2, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, 0)) == MAP_FAILED)
		return NULL;

	database->file->active_page = 1;

	memset(((char *) database->file) + PAGE_SIZE, 0, PAGE_SIZE);

	return database;
}

void database_close(database_t *database)
{
	int r;
	if ((r = munmap(database, PAGE_SIZE)) == -1)
		return;
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
