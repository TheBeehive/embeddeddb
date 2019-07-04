#include <stdlib.h>
#include <string.h>

#include "database.h"


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
	strcpy((char *)transaction->data, "something\n");
	commit_transaction(db, transaction);
	database_close(db);

    return EXIT_SUCCESS;
}
