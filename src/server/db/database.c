#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include "database.h"
#include "config.h"

PGconn* db_connect() {
    char conninfo[256];
    snprintf(conninfo, sizeof(conninfo), "host=%s port=%d dbname=%s user=%s password=%s",
             DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASS);

    PGconn* conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "[DB] Connection to database failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }
    printf("[DB] Connected to database successfully.\n");
    return conn;
}

void db_disconnect(PGconn *conn) {
    if (conn != NULL) {
        PQfinish(conn);
    }
}

PGresult* db_execute(PGconn *conn, const char *query, int param_count, const char *params[]) {
    if (conn == NULL || query == NULL) {
        return NULL;
    }

    PGresult *result = PQexecParams(conn, query, param_count, NULL, params, NULL, NULL, 0);

    if (result == NULL) {
        fprintf(stderr, "[DB] Query execution failed: %s\n", PQerrorMessage(conn));
        return NULL;
    }

    ExecStatusType status = PQresultStatus(result);

    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        fprintf(stderr, "[DB] Query failed: %s\n", PQresultErrorMessage(result));
        PQclear(result);
        return NULL;
    }

    return result;
}


