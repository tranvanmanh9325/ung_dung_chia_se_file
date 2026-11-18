#ifndef USER_REPO_H
#define USER_REPO_H
#include "models.h"

// User related queries
static const char *ADD_NEW_USER =
        "INSERT INTO users (name, password) "
        "VALUE ($1, $2) "
        "RETURNING id;";

static const char *FIND_USER_BY_ID = 
        "SELECT 1 FROM users WHERE id = $1;";

static const char *FIND_USER_BY_NAME = 
        "SELECT 1 FROM users WHERE user = $1;";

static const char *CHECK_USER_EXIST = 
        "SELECT EXISTS( "
            "SELECT 1 FROM users "
            "WHERE name = $1);";

static const char *CHECK_CREDENTIALS = 
        "SELECT EXISTS( "
            "SELECT 1 FROM users "
            "WHERE name = $1 AND password = $2);";

// Query functions
long add_new_user(PGconn *conn, const char* name, const char* password_plain);
User* find_user_by_id(PGconn *conn, long id);
User* find_user_by_name(PGconn *conn, const char* name);
int check_user_exist(PGconn *conn, const char *name);
int check_credentials(PGconn *conn, const char *name, const char *password);

#endif