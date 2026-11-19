#include <stdio.h>
#include <stdbool.h>
#include <libpq-fe.h>
#include <sodium.h>
#include "../database.h"
#include "user_repo.h"

// Check if username exists
void check_username_exist(PGconn* conn, const char* name, bool* res) {
    const char *params[] = {name};
    
    PGresult* result;
    if(db_execute(conn, QUERY_CHECK_USERNAME_EXIST, 1, params, &result) == DB_EXECUTE_FAILED) {
        PQclear(result);
        fprintf(stderr, "[DB] Something went wrong. Please try again.");
        return false;
    };

    char* value = PQgetvalue(result, 0, 0);

    if (strcmp(value, "t") == 0) {
        *res = true;
    } else if (strcmp(value, "f") == 0) {
        *res = false;
    } else {
        *res = false;
    }
    PQclear(result);
}

// Register a new account
void register_account(PGconn* conn, const char* name, const char* password_plain, User* user) {

    char password_hash[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(
        password_hash,
        password_plain,
        strlen(password_plain),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE
    ) != 0) {
        fprintf(stderr, "[DB] Password encryption failed.");
        return;
    }

    const char *params[] = {name, password_hash};

    PGresult* result;
    if (db_execute(conn, QUERY_REGISTER_ACCOUNT, 2, params, &result) == DB_EXECUTE_FAILED) {
        PQclear(result);
        fprintf(stderr, "[DB] Something went wrong. Please try again.");
        return false;
    }

    user->id = atol(PQgetvalue(result, 0, 0));
    snprintf(user->name, sizeof(user->name), "%s", PQgetvalue(result, 0, 1));
    snprintf(user->created_at, sizeof(user->created_at), "%s", PQgetvalue(result, 0, 2));
    PQclear(result);
}

// Check credentials
void check_credentials(PGconn* conn, const char* name, const char* password_plain, bool* check) {
    const char *params[] = {name};

    // Get name and password hash of the input name
    PGresult* result;
    if (db_execute(conn, QUERY_REGISTER_ACCOUNT, 1, params, &result) == DB_EXECUTE_FAILED) {
        PQclear(result);
        fprintf(stderr, "[DB] Something went wrong. Please try again.");
        *check = false;
    }

    // Name doesn't exist
    if (PQntuples(result) <= 0) {
        PQclear(result);
        *check = false;
    }

    // If name exists, check if the password hash matches the plain
    char* password_hash = PQgetvalue(result, 0, 1);
    if (crypto_pwhash_str_verify(password_hash, password_plain, strlen(password_plain)) == 0) {
        PQclear(result);
        *check = true;
    } else {
        PQclear(result);
        *check = false;
    }

}