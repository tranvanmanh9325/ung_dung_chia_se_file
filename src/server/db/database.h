#ifndef DATABASE_H
#define DATABASE_H
#include <libpq-fe.h>

// Connect to database
PGconn* db_connect(void);

// Disconnect from database
void db_disconnect(PGconn* conn);

// Execute a query with number of params
PGresult* db_execute(PGconn *conn, const char *query, int param_count, const char *params[]);

// Group related queries
static const char *FIND_GROUP_BY_ID =
        "SELECT 1 FROM groups WHERE id = $1";

static const char *FIND_GROUP_BY_NAME =
        "SELECT 1 FROM groups WHERE name = $1";

static const char *GET_GROUP_MEMBERS =
        "SELECT users.* FROM users "
        "JOIN users_groups "
            "ON users.id = users_groups.user_id "
        "WHERE users_groups.group_id = $1";

#endif