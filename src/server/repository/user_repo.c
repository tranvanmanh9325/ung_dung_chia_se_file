#include <stdlib.h>
#include <stdio.h>
#include <sodium.h>
#include <time.h>
#include <libpq-fe.h>
#include "user_repo.h"
#include "../db/database.h"

long add_new_user(PGconn *conn, const char* name, const char* password_plain) {

    char password_hash[crypto_pwhash_STRBYTES];

    crypto_pwhash_str(password_hash, password_plain, strlen(password_plain),
                    crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE);

    const char *params[] = { name, password_hash };
    PGresult *result = db_execute(conn, ADD_NEW_USER, 2, params);

    // If result is false or has 0 rows
    if (result == NULL || PQntuples(result) == 0) {
        if (result != NULL) PQclear(result);
        return NULL;
    }

    return atol(PQgetvalue(result, 0, 0));
}

User* find_user_by_id(PGconn *conn, long id) {

    char id_str[10];
    snprintf(id_str, sizeof(id_str), "%d", id);

    const char *params[] = { id_str };
    PGresult *result = db_execute(conn, FIND_USER_BY_ID, 1, params);

    // If result is false or has 0 rows
    if (result == NULL || PQntuples(result) == 0) {
        if (result != NULL) PQclear(result);
        return NULL;
    }

    User *user = malloc(sizeof(User));

    // If user struct creation fails
    if (user == NULL) {
        PQclear(result);
        return NULL;
    }

    user->id = atol(PQgetvalue(result, 0, 0));
    strcpy(user->name, PQgetvalue(result, 0, 1));
    strcpy(user->password, PQgetvalue(result, 0, 2));
    strcpy(user->created_at, PQgetvalue(result, 0, 3));
    strcpy(user->last_login_at, PQgetvalue(result, 0, 4));

    PQclear(result);
    return user;
}

User* find_user_by_name(PGconn *conn, const char* name) {

    const char *params[] = { name };
    PGresult *result = db_execute(conn, FIND_USER_BY_NAME, 1, params);

    // If result is false or has 0 rows
    if (result == NULL || PQntuples(result) == 0) {
        if (result != NULL) PQclear(result);
        return NULL;
    }

    User *user = malloc(sizeof(User));

    // If user struct creation fails
    if (user == NULL) {
        PQclear(result);
        return NULL;
    }

    user->id = atol(PQgetvalue(result, 0, 0));
    strcpy(user->name, PQgetvalue(result, 0, 1));
    strcpy(user->password, PQgetvalue(result, 0, 2));
    strcpy(user->created_at, PQgetvalue(result, 0, 3));
    strcpy(user->last_login_at, PQgetvalue(result, 0, 4));

    PQclear(result);
    return user;
}

int check_user_exist(PGconn *conn, const char *name) {

    const char *params[] = { name };
    PGresult *result = db_execute(conn, CHECK_USER_EXIST, 1, params);

    // If result is NULL or has 0 rows
    if (result == NULL || PQntuples(result) == 0) {
        if (result != NULL) PQclear(result);
        return -1;
    }

    if (strcmp(PQgetvalue(result, 0, 0), "t") == 0) {
        PQclear(result);
        return 1; // User exists
    } else {
        PQclear(result);
        return 0; // User does not exist
    }

}

int check_credentials(PGconn *conn, const char *name, const char *password) {

    const char *params[] = { name, password };
    PGresult *result = db_execute(conn, CHECK_CREDENTIALS, 2, params);

    // If result is NULL or has 0 rows
    if (result == NULL || PQntuples(result) == 0) {
        if (result != NULL) PQclear(result);
        return -1;
    }

    if (strcmp(PQgetvalue(result, 0, 0), "t") == 0) {
        PQclear(result);
        return 1; // Credentials are valid
    } else {
        PQclear(result);
        return 0; // Credentials are invalid
    }
}

