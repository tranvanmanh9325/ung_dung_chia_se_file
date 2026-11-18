#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sodium.h>
#include <libpq-fe.h>
#include "user_repo.h"
#include "../db/database.h"

// Register a new user
long add_new_user(PGconn *conn, const char *name, const char *password_plain) {
    char password_hash[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(
            password_hash,
            password_plain,
            strlen(password_plain),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE
        ) != 0) {
        return -1;
    }

    const char *params[] = { name, password_hash };

    PGresult *result = db_execute(conn, ADD_NEW_USER, 2, params);

    if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        if (result != NULL) PQclear(result);
        return -1;
    }

    long id = atol(PQgetvalue(result, 0, 0));
    PQclear(result);
    return id;
}

// Find user by ID
User* find_user_by_id(PGconn *conn, long id) {
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%ld", id);

    const char *params[] = { id_str };

    PGresult *result = db_execute(conn, FIND_USER_BY_ID, 1, params);

    if (result == NULL ||
        PQresultStatus(result) != PGRES_TUPLES_OK ||
        PQntuples(result) == 0) {

        if (result != NULL) PQclear(result);
        return NULL;
    }

    User *user = malloc(sizeof(User));

    if (user == NULL) {
        PQclear(result);
        return NULL;
    }

    user->id = atol(PQgetvalue(result, 0, 0));
    snprintf(user->name, sizeof(user->name), "%s", PQgetvalue(result, 0, 1));
    snprintf(user->created_at, sizeof(user->created_at), "%s", PQgetvalue(result, 0, 2));

    if (PQgetisnull(result, 0, 3)) {
        user->last_login_at[0] = '\0';
    } else {
        snprintf(user->last_login_at, sizeof(user->last_login_at), "%s", PQgetvalue(result, 0, 3));
    }

    PQclear(result);
    return user;
}

// Find user by username
User* find_user_by_name(PGconn *conn, const char *name) {
    const char *params[] = { name };
    PGresult *result = db_execute(conn, FIND_USER_BY_NAME, 1, params);

    if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        if (result != NULL) PQclear(result);
        return NULL;
    }

    User *user = malloc(sizeof(User));

    if (user == NULL) {
        PQclear(result);
        return NULL;
    }

    user->id = atol(PQgetvalue(result, 0, 0));
    snprintf(user->name, sizeof(user->name), "%s", PQgetvalue(result, 0, 1));
    snprintf(user->created_at, sizeof(user->created_at), "%s", PQgetvalue(result, 0, 2));
    if (PQgetisnull(result, 0, 3)) {
        user->last_login_at[0] = '\0';
    } else {
        snprintf(user->last_login_at, sizeof(user->last_login_at), "%s", PQgetvalue(result, 0, 3));
    }

    PQclear(result);
    return user;
}

// Check whether username exists
int check_user_exist(PGconn *conn, const char *name) {
    const char *params[] = { name };
    PGresult *result = db_execute(conn, CHECK_USER_EXIST, 1, params);

    if (result == NULL ||
        PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {

        if (result != NULL) PQclear(result);
        return -1;
    }

    int exists = strcmp(PQgetvalue(result, 0, 0), "t") == 0;

    PQclear(result);
    return exists;
}

// Retrieve stored password hash for authentication
int get_user_credentials(
    PGconn *conn,
    const char *name,
    char *password_hash,
    size_t hash_size,
    long *user_id
) {
    const char *params[] = { name };
    PGresult *result = db_execute(conn, GET_USER_CREDENTIALS, 1, params);

    if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        if (result != NULL) PQclear(result);
        return -1;
    }

    const char *stored_hash = PQgetvalue(result, 0, 2);
    size_t required_size = strlen(stored_hash) + 1;

    if (required_size > hash_size) {
        PQclear(result);
        return -1;
    }

    strcpy(password_hash, stored_hash);
    *user_id = atol(PQgetvalue(result, 0, 0));
    PQclear(result);
    return 1;
}

// Update last login timestamp
int update_last_login(PGconn *conn, long id) {
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%ld", id);

    const char *params[] = { id_str };
    PGresult *result = db_execute(conn, UPDATE_LAST_LOGIN, 1, params);

    if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        if (result != NULL) PQclear(result);
        return -1;
    }

    PQclear(result);
    return 1;
}


