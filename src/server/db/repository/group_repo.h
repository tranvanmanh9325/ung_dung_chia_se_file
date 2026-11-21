#ifndef GROUP_REPO_H
#define GROUP_REPO_H
#include <libpq-fe.h>
#include "models.h"

static const char* QUERY_GET_ALL_GROUP_MEMBERS = 
    "SELECT id, name, last_login_at FROM users "
    "JOIN users_groups "
        "ON users.id = users_groups.user_id "
    "WHERE users_groups.group_id = $1;";

static const char* QUERY_GET_ALL_GROUPS_OF_USER = 
    "SELECT groups.* FROM groups "
    "JOIN users_groups "
        "ON groups.id = users_groups.group_id "
    "WHERE users_groups.user_id = $1; ";

static const char* QUERY_GET_ALL_PENDING_REQUESTS =
    "SELECT * FROM requests "
    "WHERE group_id = $1 "
        "AND status = 'PENDING';";

static const char* QUERY_PROCESS_REQUEST =
    "WITH processed AS ("
        "UPDATE requests "
        "SET status = $1 "
        "WHERE id = $2 AND status = 'PENDING' "
        "RETURNING requester_id, group_id"
    ") "
    "INSERT INTO users_groups (user_id, group_id) "
    "SELECT requester_id, group_id FROM processed "
    "WHERE $1 = 'ACCEPTED' "
    "RETURNING user_id;";

static const char* QUERY_REMOVE_USER_FROM_GROUP =
    "DELETE FROM users_groups "
    "WHERE user_id = $1 AND group_id = $2; "
    "RETURNING user_id, group_id;";


void get_all_group_members(PGconn* conn, long group_id, User** members);
void get_all_groups_of_user(PGconn* conn, long user_id, Group** groups);
void get_all_pending_requests(PGconn* conn, long group_id, Request** requests);
void process_request(PGconn* conn, long request_id, const char* action);
void remove_user_from_group(PGconn* conn, long user_id, long group_id);

#endif