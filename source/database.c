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
	transaction->read_page = database->file->active_page;
	transaction->data = (char *) database->file + get_page_offset(database->file->active_page);
	database->refcount[transaction->read_page] += 1;
	return transaction;
}

static void commit_read_transaction(database_t *database, transaction_t *transaction)
{
	if (transaction->read_page > 100)
		exit(1);
	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

static void cancel_read_transaction(database_t *database, transaction_t *transaction)
{
	if (transaction->read_page > 100)
		exit(1);
	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

static transaction_t *start_write_transaction(database_t *database)
{
	struct stat st;
	int r = fstat(database->fd, &st);

	transaction_t *transaction = malloc(sizeof(transaction_t));
	transaction->write_page = 101;
	for (size_t i = 0; i < 100; i++)
	{
		if (database->refcount[i] == 0)
		{
			transaction->write_page = i;
			break;
		}
	}

	if (transaction->write_page == 101)
	{
		/* no available pages. resize the file to have a new page */
		ftruncate(database->fd, st.st_size + PAGE_SIZE);
		transaction->write_page = get_page_number(st.st_size);
		// TODO: change hard-coded 100 and re-size refcount array here if new
		// page number exceeds 100
		if (transaction->write_page > 100)
			exit(1);
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
	// TODO: should I instead keep updating database->file to point to the active_page
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

	if (transaction->read_page > 100)
		exit(1);
	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

static void cancel_read_write_transaction(database_t *database, transaction_t *transaction)
{
	if (transaction->read_page > 100)
		exit(1);
	database->refcount[transaction->read_page] -= 1;
	free(transaction);
}

database_t *database_new()
{
	if (PAGE_SIZE < sizeof(database_file_t))
		exit(1);

	database_t *database = malloc(sizeof(database_t));
	database->refcount = malloc(sizeof(int) * 100);
	for (size_t i = 0; i < 100; i++)
	{
		database->refcount[i] = 0;
	}

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
	// do I need to explicitly free each member of the refcount array?
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
