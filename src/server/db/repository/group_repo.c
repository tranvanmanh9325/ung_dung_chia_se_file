#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include "group_repo.h"
#include "../database.h"

void get_all_group_members(PGconn* conn, long group_id, User** members) {
    *members = NULL;
    char group_id_str[32];
    snprintf(group_id_str, sizeof(group_id_str), "%ld", group_id);
    const char* params[] = { group_id_str };
    PGresult* result = NULL;

    if (db_execute(conn, QUERY_GET_ALL_GROUP_MEMBERS, 1, params, &result) == DB_EXECUTE_FAILED)
        return;

    int count = PQntuples(result);
    *members = calloc(count + 1, sizeof(User));

    if (*members == NULL) {
        PQclear(result);
        return;
    }

    for (int i = 0; i < count; i++) {
        (*members)[i].id = atol(PQgetvalue(result, i, 0));
        snprintf((*members)[i].name, sizeof((*members)[i].name), "%s", PQgetvalue(result, i, 1));
        if (!PQgetisnull(result, i, 2))
            snprintf((*members)[i].last_login_at, sizeof((*members)[i].last_login_at), "%s", PQgetvalue(result, i, 2));
    }

    PQclear(result);
}

void get_all_groups_of_user(PGconn* conn, long user_id, Group** groups) {
    *groups = NULL;
    char user_id_str[32];
    snprintf(user_id_str, sizeof(user_id_str), "%ld", user_id);
    const char* params[] = { user_id_str };
    PGresult* result = NULL;

    if (db_execute(conn, QUERY_GET_ALL_GROUPS_OF_USER, 1, params, &result) == DB_EXECUTE_FAILED)
        return;

    // Get number of rows returned by the query and allocate memory for the groups array
    int count = PQntuples(result);
    *groups = calloc(count + 1, sizeof(Group));

    if (*groups == NULL) {
        PQclear(result);
        return;
    }

    for (int i = 0; i < count; i++) {
        (*groups)[i].id = atol(PQgetvalue(result, i, 0));
        snprintf((*groups)[i].name, sizeof((*groups)[i].name), "%s", PQgetvalue(result, i, 1));
        snprintf((*groups)[i].dir_path, sizeof((*groups)[i].dir_path), "%s", PQgetvalue(result, i, 2));
        (*groups)[i].owner_id = atol(PQgetvalue(result, i, 3));
        snprintf((*groups)[i].created_at, sizeof((*groups)[i].created_at), "%s", PQgetvalue(result, i, 4));
        snprintf((*groups)[i].updated_at, sizeof((*groups)[i].updated_at), "%s", PQgetvalue(result, i, 5));
    }

    PQclear(result);
}

void get_all_pending_requests(PGconn* conn, long group_id, Request** requests) {
    *requests = NULL;
    char group_id_str[32];
    snprintf(group_id_str, sizeof(group_id_str), "%ld", group_id);
    const char* params[] = { group_id_str };
    PGresult* result = NULL;

    if (db_execute(conn, QUERY_GET_ALL_PENDING_REQUESTS, 1, params, &result) == DB_EXECUTE_FAILED)
        return;

    int count = PQntuples(result);
    *requests = calloc(count + 1, sizeof(Request));

    if (*requests == NULL) {
        PQclear(result);
        return;
    }

    for (int i = 0; i < count; i++) {
        (*requests)[i].id = atol(PQgetvalue(result, i, 0));
        snprintf((*requests)[i].message, sizeof((*requests)[i].message), "%s", PQgetvalue(result, i, 1));
        (*requests)[i].requester_id = atol(PQgetvalue(result, i, 2));
        (*requests)[i].group_id = atol(PQgetvalue(result, i, 3));
        snprintf((*requests)[i].status, sizeof((*requests)[i].status), "%s", PQgetvalue(result, i, 4));
        snprintf((*requests)[i].created_at, sizeof((*requests)[i].created_at), "%s", PQgetvalue(result, i, 5));
    }

    PQclear(result);
}

void process_request(PGconn* conn, long request_id, const char* action) {
    if (action == NULL || (strcmp(action, "ACCEPTED") != 0 && strcmp(action, "REJECTED") != 0))
        return;

    char request_id_str[32];
    snprintf(request_id_str, sizeof(request_id_str), "%ld", request_id);

    const char* params[] = { action, request_id_str };
    PGresult* result = NULL;

    if (db_execute(conn, QUERY_PROCESS_REQUEST, 2, params, &result) == DB_EXECUTE_FAILED)
        return;

    PQclear(result);
}

void remove_user_from_group(PGconn* conn, long user_id, long group_id) {
    char user_id_str[32];
    char group_id_str[32];
    snprintf(user_id_str, sizeof(user_id_str), "%ld", user_id);
    snprintf(group_id_str, sizeof(group_id_str), "%ld", group_id);

    const char* params[] = { user_id_str, group_id_str };
    PGresult* result = NULL;

    if (db_execute(conn, QUERY_REMOVE_USER_FROM_GROUP, 2, params, &result) == DB_EXECUTE_FAILED)
        return;

    PQclear(result);
}