#ifndef DATABASE_H
#define DATABASE_H

#include <stddef.h>

typedef struct database_file_t database_file_t;

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
	size_t write_page;
	size_t read_page;
	TRANSACTION_MODE tm;
} transaction_t;

database_t *database_new();
void database_close(database_t *database);

transaction_t *start_transaction(database_t *database, TRANSACTION_MODE tm);
void commit_transaction(database_t *database, transaction_t *transaction);
void cancel_transaction(database_t *database, transaction_t *transaction);

#endif /* DATABASE_H */
