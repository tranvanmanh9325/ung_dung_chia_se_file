#ifndef USER_REPO_H
#define USER_REPO_H
#include "models.h"
#include <stdbool.h>
#include <libpq-fe.h>

// Queries
static const char* QUERY_CHECK_USERNAME_EXIST =
    "SELECT EXISTS ( "
        "SELECT 1 "
        "FROM users "
        "WHERE name = $1 "
    ");";

static const char* QUERY_REGISTER_ACCOUNT =
    "INSERT INTO users (name, password) "
    "VALUES ($1, $2) "
    "RETURNING id, name, created_at;";

static const char* QUERY_GET_USER_AUTHENTICATION =
    "SELECT name, password "
    "FROM users "
    "WHERE name = $1;";

static const char* QUERY_UPDATE_LAST_LOGIN = 
    "UPDATE users "
    "SET last_login_at = CURRENT_TIMESTAMP "
    "WHERE id = $1 "
    "RETURNING last_login_at; ";

void check_username_exist(PGconn* conn, const char* name, bool* res);
void register_account(PGconn* conn, const char* name, const char* password_plain, User* user);
void check_credentials(PGconn* conn, const char* name, const char* password_plain, bool* check);

#endif