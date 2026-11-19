#ifndef DATABASE_H
#define DATABASE_H
#include <libpq-fe.h>

typedef enum {
    DB_CONN_SUCCESS,
    DB_CONN_FAILED,
    DB_EXECUTE_SUCCESS,
    DB_EXECUTE_FAILED
} Status;

// Connect to database
PGconn* db_connect(void);

// Disconnect from database
void db_disconnect(PGconn* conn);

// Execute a query with number of params
Status db_execute(PGconn *conn, const char *query, int param_count, const char *params[], PGresult* result);

#endif