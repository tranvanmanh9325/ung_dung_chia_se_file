#include "db.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================== Helpers noi bo ===================== */

static void copy_str(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%s", src);
}

static bool check_conn(PGconn *conn)
{
    return conn != NULL && PQstatus(conn) == CONNECTION_OK;
}

static bool exec_simple_cmd(PGconn *conn, const char *sql)
{
    PGresult *res = PQexec(conn, sql);
    if (res == NULL) {
        return false;
    }
    ExecStatusType st = PQresultStatus(res);
    PQclear(res);
    return (st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK);
}

static void fill_user_row(PGresult *res, int row, db_user_t *u)
{
    if (u == NULL) {
        return;
    }
    memset(u, 0, sizeof(*u));
    u->id = strtoll(PQgetvalue(res, row, 0), NULL, 10);
    copy_str(u->name, sizeof(u->name), PQgetvalue(res, row, 1));
    copy_str(u->created_at, sizeof(u->created_at), PQgetvalue(res, row, 2));
    if (!PQgetisnull(res, row, 3)) {
        copy_str(u->last_login_at, sizeof(u->last_login_at), PQgetvalue(res, row, 3));
    }
}

static void fill_group_row(PGresult *res, int row, db_group_t *g)
{
    if (g == NULL) {
        return;
    }
    memset(g, 0, sizeof(*g));
    g->id = strtoll(PQgetvalue(res, row, 0), NULL, 10);
    copy_str(g->name, sizeof(g->name), PQgetvalue(res, row, 1));
    copy_str(g->dir_path, sizeof(g->dir_path), PQgetvalue(res, row, 2));
    g->owner_id = strtoll(PQgetvalue(res, row, 3), NULL, 10);
    copy_str(g->created_at, sizeof(g->created_at), PQgetvalue(res, row, 4));
    copy_str(g->updated_at, sizeof(g->updated_at), PQgetvalue(res, row, 5));
}

static void fill_entry_row(PGresult *res, int row, db_entry_t *e)
{
    if (e == NULL) {
        return;
    }
    memset(e, 0, sizeof(*e));
    e->id = strtoll(PQgetvalue(res, row, 0), NULL, 10);
    copy_str(e->name, sizeof(e->name), PQgetvalue(res, row, 1));
    copy_str(e->type, sizeof(e->type), PQgetvalue(res, row, 2));
    copy_str(e->path, sizeof(e->path), PQgetvalue(res, row, 3));
    e->uploader_id = strtoll(PQgetvalue(res, row, 4), NULL, 10);
    e->group_id = strtoll(PQgetvalue(res, row, 5), NULL, 10);
    if (PQgetisnull(res, row, 6)) {
        e->parent_id = 0;
    } else {
        e->parent_id = strtoll(PQgetvalue(res, row, 6), NULL, 10);
    }
    copy_str(e->uploaded_at, sizeof(e->uploaded_at), PQgetvalue(res, row, 7));
}

/* ===================== 0. Ket noi & Tien ich ===================== */

PGconn *db_connect(const char *conninfo)
{
    if (conninfo == NULL) {
        return NULL;
    }
    PGconn *conn = PQconnectdb(conninfo);
    if (conn == NULL || PQstatus(conn) != CONNECTION_OK) {
        if (conn != NULL) {
            fprintf(stderr, "[DB] Connection failed: %s\n", PQerrorMessage(conn));
            PQfinish(conn);
        }
        return NULL;
    }
    return conn;
}

void db_disconnect(PGconn *conn)
{
    if (conn != NULL) {
        PQfinish(conn);
    }
}

db_status_t db_exec_sql_file(PGconn *conn, const char *filepath)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (filepath == NULL || filepath[0] == '\0') {
        return DB_ERR_INVALID_ARG;
    }

    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return DB_ERR_QUERY;
    }
    long len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        return DB_ERR_QUERY;
    }
    rewind(fp);

    char *sql = malloc((size_t)len + 1);
    if (sql == NULL) {
        fclose(fp);
        return DB_ERR_QUERY;
    }

    size_t read_bytes = fread(sql, 1, (size_t)len, fp);
    fclose(fp);
    sql[read_bytes] = '\0';

    bool ok = exec_simple_cmd(conn, sql);
    free(sql);

    return ok ? DB_OK : DB_ERR_QUERY;
}

const char *db_status_str(db_status_t status)
{
    switch (status) {
    case DB_OK:
        return "OK";
    case DB_ERR_INVALID_ARG:
        return "Tham so khong hop le";
    case DB_ERR_CONN:
        return "Loi ket noi CSDL";
    case DB_ERR_QUERY:
        return "Loi truy van CSDL";
    case DB_ERR_NOT_FOUND:
        return "Khong tim thay du lieu";
    case DB_ERR_ALREADY_EXISTS:
        return "Du lieu da ton tai";
    case DB_ERR_AUTH_FAILED:
        return "Sai ten dang nhap hoac mat khau";
    case DB_ERR_FORBIDDEN:
        return "Khong co quyen thuc hien thao tac nay";
    default:
        return "Loi khong xac dinh";
    }
}

/* ===================== 1. Dang ky & Dang nhap ===================== */

db_status_t db_user_exists(PGconn *conn, const char *username, bool *out_exists)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (!db_is_valid_username(username) || out_exists == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    const char *sql =
        "SELECT EXISTS ("
        "    SELECT 1 FROM users WHERE name = $1"
        ");";
    const char *params[1] = { username };

    PGresult *res = PQexecParams(conn, sql, 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
        PQclear(res);
        return DB_ERR_QUERY;
    }

    *out_exists = (PQgetvalue(res, 0, 0)[0] == 't');
    PQclear(res);
    return DB_OK;
}

db_status_t db_user_register(PGconn *conn,
                             const char *username,
                             const char *plain_password,
                             db_user_t *out_user)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (!db_is_valid_username(username) ||
        plain_password == NULL || plain_password[0] == '\0') {
        return DB_ERR_INVALID_ARG;
    }

    bool exists = false;
    db_status_t st = db_user_exists(conn, username, &exists);
    if (st != DB_OK) {
        return st;
    }
    if (exists) {
        return DB_ERR_ALREADY_EXISTS;
    }

    char hashed_pw[PASSWORD_HASH_MAX_LEN];
    if (password_hash(plain_password, hashed_pw, sizeof(hashed_pw)) != 0) {
        return DB_ERR_INVALID_ARG;
    }

    const char *sql =
        "INSERT INTO users (name, password) "
        "VALUES ($1, $2) "
        "RETURNING id, name, created_at, last_login_at;";
    const char *params[2] = { username, hashed_pw };

    PGresult *res = PQexecParams(conn, sql, 2, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
        const char *sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
        bool unique_violation = (sqlstate != NULL && strcmp(sqlstate, "23505") == 0);
        PQclear(res);
        return unique_violation ? DB_ERR_ALREADY_EXISTS : DB_ERR_QUERY;
    }

    if (out_user != NULL) {
        fill_user_row(res, 0, out_user);
    }
    PQclear(res);
    return DB_OK;
}

db_status_t db_user_login(PGconn *conn,
                          const char *username,
                          const char *plain_password,
                          db_user_t *out_user)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (!db_is_valid_username(username) ||
        plain_password == NULL || plain_password[0] == '\0') {
        return DB_ERR_INVALID_ARG;
    }

    const char *sql =
        "SELECT id, name, password, created_at, last_login_at "
        "FROM users "
        "WHERE name = $1;";
    const char *params[1] = { username };

    PGresult *res = PQexecParams(conn, sql, 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DB_ERR_QUERY;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        return DB_ERR_AUTH_FAILED;
    }

    const char *stored_hash = PQgetvalue(res, 0, 2);
    if (!password_verify(plain_password, stored_hash)) {
        PQclear(res);
        return DB_ERR_AUTH_FAILED;
    }

    db_user_t user;
    memset(&user, 0, sizeof(user));
    user.id = strtoll(PQgetvalue(res, 0, 0), NULL, 10);
    copy_str(user.name, sizeof(user.name), PQgetvalue(res, 0, 1));
    copy_str(user.created_at, sizeof(user.created_at), PQgetvalue(res, 0, 3));
    if (!PQgetisnull(res, 0, 4)) {
        copy_str(user.last_login_at, sizeof(user.last_login_at), PQgetvalue(res, 0, 4));
    }
    PQclear(res);

    /* Cap nhat last_login_at */
    char id_buf[32];
    snprintf(id_buf, sizeof(id_buf), "%" PRId64, user.id);
    const char *upd_sql =
        "UPDATE users "
        "SET last_login_at = CURRENT_TIMESTAMP "
        "WHERE id = $1 "
        "RETURNING last_login_at;";
    const char *upd_params[1] = { id_buf };

    PGresult *upd_res = PQexecParams(conn, upd_sql, 1, NULL, upd_params, NULL, NULL, 0);
    if (PQresultStatus(upd_res) == PGRES_TUPLES_OK && PQntuples(upd_res) == 1) {
        copy_str(user.last_login_at, sizeof(user.last_login_at), PQgetvalue(upd_res, 0, 0));
    }
    PQclear(upd_res);

    if (out_user != NULL) {
        *out_user = user;
    }
    return DB_OK;
}

/* ===================== 2. Quan ly nhom chia se ===================== */

db_status_t db_group_create(PGconn *conn,
                            const char *group_name,
                            int64_t owner_id,
                            db_group_t *out_group)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_name == NULL || group_name[0] == '\0' ||
        strlen(group_name) >= DB_NAME_MAX || owner_id <= 0) {
        return DB_ERR_INVALID_ARG;
    }

    char owner_buf[32];
    snprintf(owner_buf, sizeof(owner_buf), "%" PRId64, owner_id);

    const char *sql =
        "WITH new_group AS ("
        "    INSERT INTO groups (name, dir_path, owner_id) "
        "    VALUES ($1, '', $2) "
        "    RETURNING id"
        "), "
        "updated_group AS ("
        "    UPDATE groups "
        "    SET dir_path = 'root/' || id "
        "    WHERE id = (SELECT id FROM new_group) "
        "    RETURNING id, name, dir_path, owner_id, created_at, updated_at"
        "), "
        "owner_membership AS ("
        "    INSERT INTO users_groups (user_id, group_id) "
        "    SELECT owner_id, id FROM updated_group"
        ") "
        "SELECT id, name, dir_path, owner_id, created_at, updated_at "
        "FROM updated_group;";

    const char *params[2] = { group_name, owner_buf };
    PGresult *res = PQexecParams(conn, sql, 2, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
        PQclear(res);
        return DB_ERR_QUERY;
    }

    if (out_group != NULL) {
        fill_group_row(res, 0, out_group);
    }
    PQclear(res);
    return DB_OK;
}

db_status_t db_group_is_owner(PGconn *conn,
                              int64_t group_id,
                              int64_t user_id,
                              bool *out_is_owner)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || user_id <= 0 || out_is_owner == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    char g_buf[32];
    char u_buf[32];
    snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);
    snprintf(u_buf, sizeof(u_buf), "%" PRId64, user_id);

    const char *sql =
        "SELECT EXISTS ("
        "    SELECT 1 FROM groups WHERE id = $1 AND owner_id = $2"
        ");";
    const char *params[2] = { g_buf, u_buf };

    PGresult *res = PQexecParams(conn, sql, 2, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
        PQclear(res);
        return DB_ERR_QUERY;
    }

    *out_is_owner = (PQgetvalue(res, 0, 0)[0] == 't');
    PQclear(res);
    return DB_OK;
}

db_status_t db_group_is_member(PGconn *conn,
                               int64_t group_id,
                               int64_t user_id,
                               bool *out_is_member)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || user_id <= 0 || out_is_member == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    char g_buf[32];
    char u_buf[32];
    snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);
    snprintf(u_buf, sizeof(u_buf), "%" PRId64, user_id);

    const char *sql =
        "SELECT EXISTS ("
        "    SELECT 1 FROM users_groups WHERE group_id = $1 AND user_id = $2"
        ");";
    const char *params[2] = { g_buf, u_buf };

    PGresult *res = PQexecParams(conn, sql, 2, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
        PQclear(res);
        return DB_ERR_QUERY;
    }

    *out_is_member = (PQgetvalue(res, 0, 0)[0] == 't');
    PQclear(res);
    return DB_OK;
}

db_status_t db_group_list_by_user(PGconn *conn,
                                  int64_t user_id,
                                  db_group_t **out_groups,
                                  size_t *out_count)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (user_id <= 0 || out_groups == NULL || out_count == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    *out_groups = NULL;
    *out_count = 0;

    char u_buf[32];
    snprintf(u_buf, sizeof(u_buf), "%" PRId64, user_id);

    const char *sql =
        "SELECT groups.id, groups.name, groups.dir_path, groups.owner_id, "
        "       groups.created_at, groups.updated_at "
        "FROM groups "
        "JOIN users_groups ON groups.id = users_groups.group_id "
        "WHERE users_groups.user_id = $1 "
        "ORDER BY groups.id ASC;";
    const char *params[1] = { u_buf };

    PGresult *res = PQexecParams(conn, sql, 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DB_ERR_QUERY;
    }

    int nrows = PQntuples(res);
    if (nrows > 0) {
        db_group_t *list = calloc((size_t)nrows, sizeof(db_group_t));
        if (list == NULL) {
            PQclear(res);
            return DB_ERR_QUERY;
        }
        for (int i = 0; i < nrows; ++i) {
            fill_group_row(res, i, &list[i]);
        }
        *out_groups = list;
        *out_count = (size_t)nrows;
    }

    PQclear(res);
    return DB_OK;
}

db_status_t db_group_list_all(PGconn *conn,
                              db_group_t **out_groups,
                              size_t *out_count)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (out_groups == NULL || out_count == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    *out_groups = NULL;
    *out_count = 0;

    const char *sql =
        "SELECT id, name, dir_path, owner_id, created_at, updated_at "
        "FROM groups "
        "ORDER BY id ASC;";

    PGresult *res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DB_ERR_QUERY;
    }

    int nrows = PQntuples(res);
    if (nrows > 0) {
        db_group_t *list = calloc((size_t)nrows, sizeof(db_group_t));
        if (list == NULL) {
            PQclear(res);
            return DB_ERR_QUERY;
        }
        for (int i = 0; i < nrows; ++i) {
            fill_group_row(res, i, &list[i]);
        }
        *out_groups = list;
        *out_count = (size_t)nrows;
    }

    PQclear(res);
    return DB_OK;
}

db_status_t db_group_list_members(PGconn *conn,
                                  int64_t group_id,
                                  int64_t caller_id,
                                  db_user_t **out_users,
                                  size_t *out_count)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || caller_id <= 0 || out_users == NULL || out_count == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    *out_users = NULL;
    *out_count = 0;

    bool is_member = false;
    db_status_t st = db_group_is_member(conn, group_id, caller_id, &is_member);
    if (st != DB_OK) {
        return st;
    }
    if (!is_member) {
        return DB_ERR_FORBIDDEN;
    }

    char g_buf[32];
    snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);

    const char *sql =
        "SELECT users.id, users.name, users.created_at, users.last_login_at "
        "FROM users "
        "JOIN users_groups ON users.id = users_groups.user_id "
        "WHERE users_groups.group_id = $1 "
        "ORDER BY users.id ASC;";
    const char *params[1] = { g_buf };

    PGresult *res = PQexecParams(conn, sql, 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DB_ERR_QUERY;
    }

    int nrows = PQntuples(res);
    if (nrows > 0) {
        db_user_t *list = calloc((size_t)nrows, sizeof(db_user_t));
        if (list == NULL) {
            PQclear(res);
            return DB_ERR_QUERY;
        }
        for (int i = 0; i < nrows; ++i) {
            fill_user_row(res, i, &list[i]);
        }
        *out_users = list;
        *out_count = (size_t)nrows;
    }

    PQclear(res);
    return DB_OK;
}

/* ===================== 3. Yeu cau tham gia nhom ===================== */

db_status_t db_request_create(PGconn *conn,
                              int64_t group_id,
                              int64_t requester_id,
                              const char *message,
                              db_request_t *out_req)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || requester_id <= 0) {
        return DB_ERR_INVALID_ARG;
    }

    bool is_member = false;
    db_status_t st = db_group_is_member(conn, group_id, requester_id, &is_member);
    if (st != DB_OK) {
        return st;
    }
    if (is_member) {
        return DB_ERR_ALREADY_EXISTS;
    }

    const char *msg = (message != NULL && message[0] != '\0')
                          ? message
                          : "Xin tham gia nhom";
    if (strlen(msg) >= DB_MSG_MAX) {
        return DB_ERR_INVALID_ARG;
    }

    char u_buf[32];
    char g_buf[32];
    snprintf(u_buf, sizeof(u_buf), "%" PRId64, requester_id);
    snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);

    const char *sql =
        "INSERT INTO requests (requester_id, group_id, message) "
        "VALUES ($1, $2, $3) "
        "RETURNING id, requester_id, group_id, message, status, created_at;";
    const char *params[3] = { u_buf, g_buf, msg };

    PGresult *res = PQexecParams(conn, sql, 3, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
        const char *sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
        bool unique_violation = (sqlstate != NULL && strcmp(sqlstate, "23505") == 0);
        bool fk_violation = (sqlstate != NULL && strcmp(sqlstate, "23503") == 0);
        PQclear(res);
        if (unique_violation) {
            return DB_ERR_ALREADY_EXISTS;
        }
        if (fk_violation) {
            return DB_ERR_NOT_FOUND;
        }
        return DB_ERR_QUERY;
    }

    if (out_req != NULL) {
        memset(out_req, 0, sizeof(*out_req));
        out_req->id = strtoll(PQgetvalue(res, 0, 0), NULL, 10);
        out_req->requester_id = strtoll(PQgetvalue(res, 0, 1), NULL, 10);
        out_req->group_id = strtoll(PQgetvalue(res, 0, 2), NULL, 10);
        copy_str(out_req->message, sizeof(out_req->message), PQgetvalue(res, 0, 3));
        copy_str(out_req->status, sizeof(out_req->status), PQgetvalue(res, 0, 4));
        copy_str(out_req->created_at, sizeof(out_req->created_at), PQgetvalue(res, 0, 5));
    }

    PQclear(res);
    return DB_OK;
}

db_status_t db_request_list_pending(PGconn *conn,
                                    int64_t group_id,
                                    int64_t leader_id,
                                    db_request_t **out_reqs,
                                    size_t *out_count)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || leader_id <= 0 || out_reqs == NULL || out_count == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    *out_reqs = NULL;
    *out_count = 0;

    bool is_owner = false;
    db_status_t st = db_group_is_owner(conn, group_id, leader_id, &is_owner);
    if (st != DB_OK) {
        return st;
    }
    if (!is_owner) {
        return DB_ERR_FORBIDDEN;
    }

    char g_buf[32];
    snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);

    const char *sql =
        "SELECT r.id, r.requester_id, u.name, r.group_id, "
        "       r.message, r.status, r.created_at "
        "FROM requests r "
        "JOIN users u ON u.id = r.requester_id "
        "WHERE r.group_id = $1 AND r.status = 'PENDING' "
        "ORDER BY r.created_at ASC;";
    const char *params[1] = { g_buf };

    PGresult *res = PQexecParams(conn, sql, 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DB_ERR_QUERY;
    }

    int nrows = PQntuples(res);
    if (nrows > 0) {
        db_request_t *list = calloc((size_t)nrows, sizeof(db_request_t));
        if (list == NULL) {
            PQclear(res);
            return DB_ERR_QUERY;
        }
        for (int i = 0; i < nrows; ++i) {
            list[i].id = strtoll(PQgetvalue(res, i, 0), NULL, 10);
            list[i].requester_id = strtoll(PQgetvalue(res, i, 1), NULL, 10);
            copy_str(list[i].requester_name, sizeof(list[i].requester_name), PQgetvalue(res, i, 2));
            list[i].group_id = strtoll(PQgetvalue(res, i, 3), NULL, 10);
            copy_str(list[i].message, sizeof(list[i].message), PQgetvalue(res, i, 4));
            copy_str(list[i].status, sizeof(list[i].status), PQgetvalue(res, i, 5));
            copy_str(list[i].created_at, sizeof(list[i].created_at), PQgetvalue(res, i, 6));
        }
        *out_reqs = list;
        *out_count = (size_t)nrows;
    }

    PQclear(res);
    return DB_OK;
}

db_status_t db_request_respond(PGconn *conn,
                               int64_t request_id,
                               int64_t leader_id,
                               bool accept)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (request_id <= 0 || leader_id <= 0) {
        return DB_ERR_INVALID_ARG;
    }

    if (!exec_simple_cmd(conn, "BEGIN")) {
        return DB_ERR_QUERY;
    }

    char r_buf[32];
    snprintf(r_buf, sizeof(r_buf), "%" PRId64, request_id);

    /* Lay thong tin request & owner cua group */
    const char *check_sql =
        "SELECT r.requester_id, r.group_id, r.status, g.owner_id "
        "FROM requests r "
        "JOIN groups g ON g.id = r.group_id "
        "WHERE r.id = $1 "
        "FOR UPDATE;";
    const char *check_params[1] = { r_buf };

    PGresult *res = PQexecParams(conn, check_sql, 1, NULL, check_params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        exec_simple_cmd(conn, "ROLLBACK");
        return DB_ERR_QUERY;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        exec_simple_cmd(conn, "ROLLBACK");
        return DB_ERR_NOT_FOUND;
    }

    int64_t requester_id = strtoll(PQgetvalue(res, 0, 0), NULL, 10);
    int64_t group_id = strtoll(PQgetvalue(res, 0, 1), NULL, 10);
    const char *status_val = PQgetvalue(res, 0, 2);
    int64_t owner_id = strtoll(PQgetvalue(res, 0, 3), NULL, 10);

    if (owner_id != leader_id) {
        PQclear(res);
        exec_simple_cmd(conn, "ROLLBACK");
        return DB_ERR_FORBIDDEN;
    }
    if (strcmp(status_val, "PENDING") != 0) {
        PQclear(res);
        exec_simple_cmd(conn, "ROLLBACK");
        return DB_ERR_ALREADY_EXISTS;
    }
    PQclear(res);

    const char *new_status = accept ? "ACCEPTED" : "REJECTED";
    const char *upd_sql =
        "UPDATE requests SET status = $2 WHERE id = $1;";
    const char *upd_params[2] = { r_buf, new_status };

    PGresult *upd_res = PQexecParams(conn, upd_sql, 2, NULL, upd_params, NULL, NULL, 0);
    if (PQresultStatus(upd_res) != PGRES_COMMAND_OK) {
        PQclear(upd_res);
        exec_simple_cmd(conn, "ROLLBACK");
        return DB_ERR_QUERY;
    }
    PQclear(upd_res);

    if (accept) {
        char u_buf[32];
        char g_buf[32];
        snprintf(u_buf, sizeof(u_buf), "%" PRId64, requester_id);
        snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);

        const char *ins_sql =
            "INSERT INTO users_groups (user_id, group_id) "
            "VALUES ($1, $2) "
            "ON CONFLICT DO NOTHING;";
        const char *ins_params[2] = { u_buf, g_buf };

        PGresult *ins_res = PQexecParams(conn, ins_sql, 2, NULL, ins_params, NULL, NULL, 0);
        if (PQresultStatus(ins_res) != PGRES_COMMAND_OK) {
            PQclear(ins_res);
            exec_simple_cmd(conn, "ROLLBACK");
            return DB_ERR_QUERY;
        }
        PQclear(ins_res);
    }

    if (!exec_simple_cmd(conn, "COMMIT")) {
        exec_simple_cmd(conn, "ROLLBACK");
        return DB_ERR_QUERY;
    }
    return DB_OK;
}

/* ===================== 4. Quan ly File & Thu muc ===================== */

db_status_t db_entry_exists_name(PGconn *conn,
                                 int64_t group_id,
                                 int64_t parent_id,
                                 const char *name,
                                 bool *out_exists)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || parent_id < 0 || !db_is_valid_entry_name(name) ||
        out_exists == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    char g_buf[32];
    char p_buf[32];
    snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);

    const char *p_param = NULL;
    if (parent_id > 0) {
        snprintf(p_buf, sizeof(p_buf), "%" PRId64, parent_id);
        p_param = p_buf;
    }

    const char *sql =
        "SELECT EXISTS ("
        "    SELECT 1 FROM entries "
        "    WHERE group_id = $1 "
        "      AND parent_id IS NOT DISTINCT FROM $2 "
        "      AND name = $3"
        ");";
    const char *params[3] = { g_buf, p_param, name };

    PGresult *res = PQexecParams(conn, sql, 3, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
        PQclear(res);
        return DB_ERR_QUERY;
    }

    *out_exists = (PQgetvalue(res, 0, 0)[0] == 't');
    PQclear(res);
    return DB_OK;
}

db_status_t db_entry_create(PGconn *conn,
                            int64_t group_id,
                            int64_t parent_id,
                            int64_t uploader_id,
                            const char *name,
                            const char *type,
                            db_entry_t *out_entry)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || parent_id < 0 || uploader_id <= 0 ||
        !db_is_valid_entry_name(name) || type == NULL) {
        return DB_ERR_INVALID_ARG;
    }
    if (strcmp(type, "FILE") != 0 && strcmp(type, "DIRECTORY") != 0) {
        return DB_ERR_INVALID_ARG;
    }

    /* Kiem tra uploader_id la thanh vien trong nhom */
    bool is_member = false;
    db_status_t st = db_group_is_member(conn, group_id, uploader_id, &is_member);
    if (st != DB_OK) {
        return st;
    }
    if (!is_member) {
        return DB_ERR_FORBIDDEN;
    }

    /* Kiem tra trung ten trong cung thu muc cha */
    bool dup = false;
    st = db_entry_exists_name(conn, group_id, parent_id, name, &dup);
    if (st != DB_OK) {
        return st;
    }
    if (dup) {
        return DB_ERR_ALREADY_EXISTS;
    }

    char u_buf[32];
    char g_buf[32];
    char p_buf[32];
    snprintf(u_buf, sizeof(u_buf), "%" PRId64, uploader_id);
    snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);

    PGresult *res = NULL;
    if (parent_id == 0) {
        /* Tao entry ngay o thu muc goc cua nhom (parent_id IS NULL) */
        const char *sql_root =
            "INSERT INTO entries (name, type, path, uploader_id, group_id, parent_id) "
            "SELECT $1, $2, g.dir_path || '/' || $1, $3, g.id, NULL "
            "FROM groups g "
            "WHERE g.id = $4 "
            "RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;";
        const char *params_root[4] = { name, type, u_buf, g_buf };
        res = PQexecParams(conn, sql_root, 4, NULL, params_root, NULL, NULL, 0);
    } else {
        /* Tao entry ben trong thu muc con parent_id */
        snprintf(p_buf, sizeof(p_buf), "%" PRId64, parent_id);
        const char *sql_sub =
            "INSERT INTO entries (name, type, path, uploader_id, group_id, parent_id) "
            "SELECT $1, $2, parent.path || '/' || $1, $3, parent.group_id, parent.id "
            "FROM entries AS parent "
            "WHERE parent.id = $4 "
            "  AND parent.group_id = $5 "
            "  AND parent.type = 'DIRECTORY' "
            "RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;";
        const char *params_sub[5] = { name, type, u_buf, p_buf, g_buf };
        res = PQexecParams(conn, sql_sub, 5, NULL, params_sub, NULL, NULL, 0);
    }

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
        bool unique_violation = (sqlstate != NULL && strcmp(sqlstate, "23505") == 0);
        PQclear(res);
        return unique_violation ? DB_ERR_ALREADY_EXISTS : DB_ERR_QUERY;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        return DB_ERR_NOT_FOUND;
    }

    if (out_entry != NULL) {
        fill_entry_row(res, 0, out_entry);
    }
    PQclear(res);
    return DB_OK;
}

db_status_t db_entry_get_by_id(PGconn *conn,
                               int64_t group_id,
                               int64_t entry_id,
                               int64_t caller_id,
                               db_entry_t *out_entry)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || entry_id <= 0 || caller_id <= 0 || out_entry == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    bool is_member = false;
    db_status_t st = db_group_is_member(conn, group_id, caller_id, &is_member);
    if (st != DB_OK) {
        return st;
    }
    if (!is_member) {
        return DB_ERR_FORBIDDEN;
    }

    char e_buf[32];
    char g_buf[32];
    snprintf(e_buf, sizeof(e_buf), "%" PRId64, entry_id);
    snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);

    const char *sql =
        "SELECT id, name, type, path, uploader_id, group_id, parent_id, uploaded_at "
        "FROM entries "
        "WHERE id = $1 AND group_id = $2;";
    const char *params[2] = { e_buf, g_buf };

    PGresult *res = PQexecParams(conn, sql, 2, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DB_ERR_QUERY;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        return DB_ERR_NOT_FOUND;
    }

    fill_entry_row(res, 0, out_entry);
    PQclear(res);
    return DB_OK;
}

db_status_t db_entry_list_children(PGconn *conn,
                                   int64_t group_id,
                                   int64_t parent_id,
                                   int64_t caller_id,
                                   db_entry_t **out_entries,
                                   size_t *out_count)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || parent_id < 0 || caller_id <= 0 ||
        out_entries == NULL || out_count == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    *out_entries = NULL;
    *out_count = 0;

    bool is_member = false;
    db_status_t st = db_group_is_member(conn, group_id, caller_id, &is_member);
    if (st != DB_OK) {
        return st;
    }
    if (!is_member) {
        return DB_ERR_FORBIDDEN;
    }

    char g_buf[32];
    char p_buf[32];
    snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);

    const char *p_param = NULL;
    if (parent_id > 0) {
        snprintf(p_buf, sizeof(p_buf), "%" PRId64, parent_id);
        p_param = p_buf;
    }

    const char *sql =
        "SELECT id, name, type, path, uploader_id, group_id, parent_id, uploaded_at "
        "FROM entries "
        "WHERE group_id = $1 AND parent_id IS NOT DISTINCT FROM $2 "
        "ORDER BY type ASC, name ASC;";
    const char *params[2] = { g_buf, p_param };

    PGresult *res = PQexecParams(conn, sql, 2, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DB_ERR_QUERY;
    }

    int nrows = PQntuples(res);
    if (nrows > 0) {
        db_entry_t *list = calloc((size_t)nrows, sizeof(db_entry_t));
        if (list == NULL) {
            PQclear(res);
            return DB_ERR_QUERY;
        }
        for (int i = 0; i < nrows; ++i) {
            fill_entry_row(res, i, &list[i]);
        }
        *out_entries = list;
        *out_count = (size_t)nrows;
    }

    PQclear(res);
    return DB_OK;
}

db_status_t db_entry_list_parent_layer(PGconn *conn,
                                       int64_t group_id,
                                       int64_t current_dir_id,
                                       int64_t caller_id,
                                       int64_t *out_new_parent_id,
                                       db_entry_t **out_entries,
                                       size_t *out_count)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || current_dir_id <= 0 || caller_id <= 0 ||
        out_entries == NULL || out_count == NULL) {
        return DB_ERR_INVALID_ARG;
    }

    db_entry_t cur;
    db_status_t st = db_entry_get_by_id(conn, group_id, current_dir_id, caller_id, &cur);
    if (st != DB_OK) {
        return st;
    }

    if (out_new_parent_id != NULL) {
        *out_new_parent_id = cur.parent_id;
    }

    return db_entry_list_children(conn, group_id, cur.parent_id, caller_id,
                                  out_entries, out_count);
}

db_status_t db_entry_rename(PGconn *conn,
                            int64_t group_id,
                            int64_t entry_id,
                            int64_t leader_id,
                            const char *new_name,
                            db_entry_t *out_entry)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || entry_id <= 0 || leader_id <= 0 ||
        !db_is_valid_entry_name(new_name)) {
        return DB_ERR_INVALID_ARG;
    }

    /* CHI TRUONG NHOM (owner_id) moi co quyen doi ten file / thu muc */
    bool is_owner = false;
    db_status_t st = db_group_is_owner(conn, group_id, leader_id, &is_owner);
    if (st != DB_OK) {
        return st;
    }
    if (!is_owner) {
        return DB_ERR_FORBIDDEN;
    }

    if (!exec_simple_cmd(conn, "BEGIN")) {
        return DB_ERR_QUERY;
    }

    char e_buf[32];
    char g_buf[32];
    snprintf(e_buf, sizeof(e_buf), "%" PRId64, entry_id);
    snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);

    const char *sel_sql =
        "SELECT id, name, type, path, uploader_id, group_id, parent_id, uploaded_at "
        "FROM entries "
        "WHERE id = $1 AND group_id = $2 "
        "FOR UPDATE;";
    const char *sel_params[2] = { e_buf, g_buf };

    PGresult *res = PQexecParams(conn, sel_sql, 2, NULL, sel_params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        exec_simple_cmd(conn, "ROLLBACK");
        return DB_ERR_QUERY;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        exec_simple_cmd(conn, "ROLLBACK");
        return DB_ERR_NOT_FOUND;
    }

    db_entry_t target;
    fill_entry_row(res, 0, &target);
    PQclear(res);

    /* Neu ten moi giong het ten cu thi tra ve luon */
    if (strcmp(target.name, new_name) == 0) {
        exec_simple_cmd(conn, "COMMIT");
        if (out_entry != NULL) {
            *out_entry = target;
        }
        return DB_OK;
    }

    /* Kiem tra trung ten trong cung thu muc cha */
    bool dup = false;
    st = db_entry_exists_name(conn, group_id, target.parent_id, new_name, &dup);
    if (st != DB_OK) {
        exec_simple_cmd(conn, "ROLLBACK");
        return st;
    }
    if (dup) {
        exec_simple_cmd(conn, "ROLLBACK");
        return DB_ERR_ALREADY_EXISTS;
    }

    /* Tinh duong dan moi dua tren vi tri dau '/' cuoi cung cua old_path */
    char new_path[DB_PATH_MAX];
    const char *last_slash = strrchr(target.path, '/');
    if (last_slash != NULL) {
        size_t prefix_len = (size_t)(last_slash - target.path);
        if (prefix_len + 1 + strlen(new_name) >= sizeof(new_path)) {
            exec_simple_cmd(conn, "ROLLBACK");
            return DB_ERR_INVALID_ARG;
        }
        memcpy(new_path, target.path, prefix_len);
        new_path[prefix_len] = '/';
        strcpy(new_path + prefix_len + 1, new_name);
    } else {
        copy_str(new_path, sizeof(new_path), new_name);
    }

    /* Buoc 1: Cap nhat ten va path cua chinh entry */
    const char *upd_sql =
        "UPDATE entries "
        "SET name = $1, path = $2 "
        "WHERE id = $3 AND group_id = $4 "
        "RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;";
    const char *upd_params[4] = { new_name, new_path, e_buf, g_buf };

    PGresult *upd_res = PQexecParams(conn, upd_sql, 4, NULL, upd_params, NULL, NULL, 0);
    if (PQresultStatus(upd_res) != PGRES_TUPLES_OK || PQntuples(upd_res) != 1) {
        PQclear(upd_res);
        exec_simple_cmd(conn, "ROLLBACK");
        return DB_ERR_QUERY;
    }

    db_entry_t updated_entry;
    fill_entry_row(upd_res, 0, &updated_entry);
    PQclear(upd_res);

    /* Buoc 2: Neu la DIRECTORY, cap nhat path cho toan bo cac entry con/chau ben duoi */
    if (strcmp(target.type, "DIRECTORY") == 0) {
        const char *child_sql =
            "UPDATE entries "
            "SET path = $1 || SUBSTRING(path FROM CHAR_LENGTH($2) + 1) "
            "WHERE group_id = $3 "
            "  AND LEFT(path, CHAR_LENGTH($2) + 1) = ($2 || '/');";
        const char *child_params[3] = { new_path, target.path, g_buf };

        PGresult *child_res = PQexecParams(conn, child_sql, 3, NULL, child_params, NULL, NULL, 0);
        if (PQresultStatus(child_res) != PGRES_COMMAND_OK) {
            PQclear(child_res);
            exec_simple_cmd(conn, "ROLLBACK");
            return DB_ERR_QUERY;
        }
        PQclear(child_res);
    }

    if (!exec_simple_cmd(conn, "COMMIT")) {
        exec_simple_cmd(conn, "ROLLBACK");
        return DB_ERR_QUERY;
    }

    if (out_entry != NULL) {
        *out_entry = updated_entry;
    }
    return DB_OK;
}

db_status_t db_entry_delete(PGconn *conn,
                            int64_t group_id,
                            int64_t entry_id,
                            int64_t leader_id,
                            db_entry_t *out_deleted)
{
    if (!check_conn(conn)) {
        return DB_ERR_CONN;
    }
    if (group_id <= 0 || entry_id <= 0 || leader_id <= 0) {
        return DB_ERR_INVALID_ARG;
    }

    /* CHI TRUONG NHOM (owner_id) moi co quyen xoa file / thu muc con */
    bool is_owner = false;
    db_status_t st = db_group_is_owner(conn, group_id, leader_id, &is_owner);
    if (st != DB_OK) {
        return st;
    }
    if (!is_owner) {
        return DB_ERR_FORBIDDEN;
    }

    char e_buf[32];
    char g_buf[32];
    snprintf(e_buf, sizeof(e_buf), "%" PRId64, entry_id);
    snprintf(g_buf, sizeof(g_buf), "%" PRId64, group_id);

    const char *del_sql =
        "DELETE FROM entries "
        "WHERE id = $1 AND group_id = $2 "
        "RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;";
    const char *params[2] = { e_buf, g_buf };

    PGresult *res = PQexecParams(conn, del_sql, 2, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DB_ERR_QUERY;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        return DB_ERR_NOT_FOUND;
    }

    if (out_deleted != NULL) {
        fill_entry_row(res, 0, out_deleted);
    }
    PQclear(res);
    return DB_OK;
}
