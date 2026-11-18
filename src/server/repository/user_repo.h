#ifndef USER_REPO_H
#define USER_REPO_H

#include "models.h"

// Register a new user
static const char *ADD_NEW_USER =
    "INSERT INTO users (name, password) "
    "VALUES ($1, $2) "
    "RETURNING id, name, created_at, last_login_at;";

// Find user by ID
static const char *FIND_USER_BY_ID =
    "SELECT id, name, created_at, last_login_at "
    "FROM users "
    "WHERE id = $1;";

// Find user by username
static const char *FIND_USER_BY_NAME =
    "SELECT id, name, created_at, last_login_at "
    "FROM users "
    "WHERE name = $1;";

// Check whether username exists
static const char *CHECK_USER_EXIST =
    "SELECT EXISTS ("
        "SELECT 1 FROM users "
        "WHERE name = $1"
    ");";

// Retrieve authentication information
static const char *GET_USER_CREDENTIALS =
    "SELECT id, name, password, created_at, last_login_at "
    "FROM users "
    "WHERE name = $1;";

// Update last login timestamp
static const char *UPDATE_LAST_LOGIN =
    "UPDATE users "
    "SET last_login_at = CURRENT_TIMESTAMP "
    "WHERE id = $1 "
    "RETURNING last_login_at;";

// Query functions

long add_new_user(PGconn *conn, const char *name, const char *password_hash);

User* find_user_by_id(PGconn *conn, long id);

User* find_user_by_name(PGconn *conn, const char *name);

int check_user_exist(PGconn *conn, const char *name);

int get_user_credentials(PGconn *conn, const char *name, char *password_hash, size_t hash_size, long *user_id);

int update_last_login(PGconn *conn, long id);

#endif
