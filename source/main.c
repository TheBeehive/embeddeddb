#include <stdlib.h>
#include <string.h>

#include "database.h"


int main(void)
{
	database_t *db = database_new("/tmp/example2");
	transaction_t *transaction = start_transaction(db, TRANSACTION_MODE_WRITE);
	strcpy((char *)transaction->data, "something\n");
	commit_transaction(db, transaction);
	database_close(db);

    return EXIT_SUCCESS;
}
