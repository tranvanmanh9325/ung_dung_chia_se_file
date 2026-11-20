#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libpq-fe.h>
#include "entry_repo.h"
#include "../database.h"

static void populate_entry(PGresult* result, int row, Entry* entry) {
    entry->id = atol(PQgetvalue(result, row, 0));
    snprintf(entry->name, sizeof(entry->name), "%s", PQgetvalue(result, row, 1));
    snprintf(entry->type, sizeof(entry->type), "%s", PQgetvalue(result, row, 2));
    snprintf(entry->path, sizeof(entry->path), "%s", PQgetvalue(result, row, 3));
    entry->uploader_id = atol(PQgetvalue(result, row, 4));
    entry->group_id = atol(PQgetvalue(result, row, 5));
    entry->parent_id = PQgetisnull(result, row, 6) ? 0 : atol(PQgetvalue(result, row, 6));
    snprintf(entry->uploaded_at, sizeof(entry->uploaded_at), "%s", PQgetvalue(result, row, 7));
}

// Get entries inside a directory
void get_entries_by_parent(PGconn* conn, long group_id, const char* parent_id, Entry** entries, int* count) {
    char group_id_str[32];
    snprintf(group_id_str, sizeof(group_id_str), "%ld", group_id);

    const char* params[] = {group_id_str, parent_id};

    *entries = NULL;
    *count = 0;

    PGresult* result;
    if (db_execute(conn, QUERY_GET_ENTRIES_BY_PARENT, 2, params, &result) == DB_EXECUTE_FAILED) {
        fprintf(stderr, "[DB] Something went wrong. Please try again.");
        return;
    }

    *count = PQntuples(result);

    if (*count > 0) {
        *entries = calloc(*count, sizeof(Entry));

        if (*entries == NULL) {
            *count = 0;
            PQclear(result);
            fprintf(stderr, "[DB] Memory allocation failed.");
            return;
        }

        for (int i = 0; i < *count; i++) {
            populate_entry(result, i, &(*entries)[i]);
        }
    }

    PQclear(result);
}

// Get entries in the parent directory
void get_parent_dir_entries(PGconn* conn, long entry_id, long group_id, Entry** entries, int* count) {
    char entry_id_str[32], group_id_str[32];
    snprintf(entry_id_str, sizeof(entry_id_str), "%ld", entry_id);
    snprintf(group_id_str, sizeof(group_id_str), "%ld", group_id);

    const char* params[] = {entry_id_str, group_id_str};

    *entries = NULL;
    *count = 0;

    PGresult* result;
    if (db_execute(conn, QUERY_GET_PARENT_DIRECTORY, 2, params, &result) == DB_EXECUTE_FAILED) {
        fprintf(stderr, "[DB] Something went wrong. Please try again.");
        return;
    }

    *count = PQntuples(result);

    if (*count > 0) {
        *entries = calloc(*count, sizeof(Entry));

        if (*entries == NULL) {
            *count = 0;
            PQclear(result);
            fprintf(stderr, "[DB] Memory allocation failed.");
            return;
        }

        for (int i = 0; i < *count; i++) {
            populate_entry(result, i, &(*entries)[i]);
        }
    }

    PQclear(result);
}

// Check if an entry already exists
void check_duplicate_entry(PGconn* conn, long group_id, const char* parent_id, const char* name, const char* type, bool* res) {
    char group_id_str[32];
    snprintf(group_id_str, sizeof(group_id_str), "%ld", group_id);

    const char* params[] = {group_id_str, parent_id, name, type};

    *res = false;

    PGresult* result;
    if (db_execute(conn, QUERY_CHECK_DUPLICATE_ENTRY, 4, params, &result) == DB_EXECUTE_FAILED) {
        fprintf(stderr, "[DB] Something went wrong. Please try again.");
        return;
    }

    if (PQntuples(result) > 0) {
        *res = strcmp(PQgetvalue(result, 0, 0), "t") == 0;
    }

    PQclear(result);
}

// Insert a new entry
void insert_entry(PGconn* conn, const char* name, const char* type, const char* parent_id, long uploader_id, long group_id, Entry** entry) {
    char uploader_id_str[32], group_id_str[32];
    snprintf(uploader_id_str, sizeof(uploader_id_str), "%ld", uploader_id);
    snprintf(group_id_str, sizeof(group_id_str), "%ld", group_id);

    const char* params[] = {name, type, parent_id, uploader_id_str, group_id_str};

    *entry = NULL;

    PGresult* result;
    if (db_execute(conn, QUERY_INSERT_ENTRY, 5, params, &result) == DB_EXECUTE_FAILED) {
        fprintf(stderr, "[DB] Something went wrong. Please try again.");
        return;
    }

    if (PQntuples(result) <= 0) {
        PQclear(result);
        fprintf(stderr, "[DB] Entry could not be created.");
        return;
    }

    *entry = malloc(sizeof(Entry));

    if (*entry == NULL) {
        PQclear(result);
        fprintf(stderr, "[DB] Memory allocation failed.");
        return;
    }

    populate_entry(result, 0, *entry);
    PQclear(result);
}

// Rename an entry
void rename_entry(PGconn* conn, long entry_id, long group_id, const char* new_name, const char* new_path) {
    char entry_id_str[32], group_id_str[32];
    snprintf(entry_id_str, sizeof(entry_id_str), "%ld", entry_id);
    snprintf(group_id_str, sizeof(group_id_str), "%ld", group_id);

    const char* params[] = {new_name, new_path, entry_id_str, group_id_str};

    PGresult* result;
    if (db_execute(conn, QUERY_RENAME_ENTRY, 4, params, &result) == DB_EXECUTE_FAILED) {
        fprintf(stderr, "[DB] Something went wrong. Please try again.");
        return;
    }

    if (PQntuples(result) <= 0) {
        fprintf(stderr, "[DB] Entry not found.");
    }

    PQclear(result);
}

// Update paths of all descendants after renaming a directory
void update_descendant_paths(PGconn* conn, const char* new_path, const char* old_path, long group_id) {
    char group_id_str[32];
    snprintf(group_id_str, sizeof(group_id_str), "%ld", group_id);

    const char* params[] = {new_path, old_path, group_id_str};

    PGresult* result;
    if (db_execute(conn, QUERY_UPDATE_DESCENDANT_PATHS, 3, params, &result) == DB_EXECUTE_FAILED) {
        fprintf(stderr, "[DB] Something went wrong. Please try again.");
        return;
    }

    PQclear(result);
}

// Delete an entry
void delete_entry(PGconn* conn, long entry_id, long group_id, Entry** deleted_entry) {
    char entry_id_str[32], group_id_str[32];
    snprintf(entry_id_str, sizeof(entry_id_str), "%ld", entry_id);
    snprintf(group_id_str, sizeof(group_id_str), "%ld", group_id);

    const char* params[] = {entry_id_str, group_id_str};

    *deleted_entry = NULL;

    PGresult* result;
    if (db_execute(conn, QUERY_DELETE_ENTRY, 2, params, &result) == DB_EXECUTE_FAILED) {
        fprintf(stderr, "[DB] Something went wrong. Please try again.");
        return;
    }

    if (PQntuples(result) <= 0) {
        PQclear(result);
        fprintf(stderr, "[DB] Entry not found.");
        return;
    }

    *deleted_entry = malloc(sizeof(Entry));

    if (*deleted_entry == NULL) {
        PQclear(result);
        fprintf(stderr, "[DB] Memory allocation failed.");
        return;
    }

    populate_entry(result, 0, *deleted_entry);
    PQclear(result);
}