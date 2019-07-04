#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define PAGE_SIZE (getpagesize())

typedef struct database_file_t
{
	size_t active_page;
} database_file_t;

typedef struct database_t {
	database_file_t *file;
	int fd;
	int *refcount;
} database_t;

typedef enum TRANSACTION_MODE
{
	TRANSACTION_MODE_READ  = (1 << 0),
	TRANSACTION_MODE_WRITE = (1 << 1),
	TRANSACTION_MODE_RW = ((1 << 0) | (1 << 1))
} TRANSACTION_MODE;

typedef struct transaction_t
{
	void *data;
	size_t page;
} transaction_t;

transaction_t *start_read_transaction(database_t *database);
transaction_t *start_read_write_transaction(database_t *database);
transaction_t *start_write_transaction(database_t *database);

off_t get_page_offset(size_t number) {
	return number * PAGE_SIZE;
}

size_t get_page_number(off_t offset) {
	return offset / PAGE_SIZE;
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

transaction_t *start_read_transaction(database_t *database)
{
	transaction_t *transaction = malloc(sizeof(transaction_t));
	transaction->page = database->file->active_page;
	transaction->data = (char *) database->file + get_page_offset(database->file->active_page);
	return transaction;
}

transaction_t *start_write_transaction(database_t *database)
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

transaction_t *start_read_write_transaction(database_t *database)
{
	transaction_t *transaction = malloc(sizeof(transaction_t));
	struct stat st;
	int r = fstat(database->fd, &st);
	ftruncate(database->fd, st.st_size + PAGE_SIZE);
	transaction->page = get_page_number(st.st_size);
	transaction->data = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, database->fd, st.st_size);
	// TODO: this doesn't work because I throw away the pointer to
	// transaction->data when I commit or cancel a transaction, so the
	// active_page may or may not be mapped, and, if it is mapped, I don't have
	// a valid pointer to it and no guarantees that the file offset num bytes
	// added to the original mapped memory when I made the database would
	// actually give me a valid memory address which points to the active page
	// in memory
	// I can either keep updating database->file to point to the active_page
	// or I can remap active_page from the file to a valid place in memory when
	// I need it
	char *active_page = (char *) database->file + get_page_offset(database->file->active_page);
	memcpy(transaction->data, active_page, PAGE_SIZE);
	return transaction;
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

void database_close(database_t *database)
{
	int r = munmap(database, PAGE_SIZE);
    printf("munmap(buf1): %i\n", r);
}

// implement garbage collection
// add refcount array to database object
// index is page number
// reads will increment it
// writes will loop through it to find the first zero refcount and then use that
// instead of what they are using now 
int main(void)
{
	database_t *db = database_new();
	transaction_t *transaction = start_transaction(db, TRANSACTION_MODE_WRITE);
	strcpy((char *)transaction->page, "something");
	commit_transaction(db, transaction);
	database_close(db);

    return EXIT_SUCCESS;
}
