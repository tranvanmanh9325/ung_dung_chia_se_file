#ifndef FILESHARE_DB_H
#define FILESHARE_DB_H

#include <libpq-fe.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "password.h"

#define DB_NAME_MAX     64
#define DB_ENTRY_MAX    256
#define DB_PATH_MAX     1024
#define DB_MSG_MAX      256
#define DB_TYPE_MAX     16
#define DB_STATUS_MAX   16
#define DB_TIME_MAX     64

typedef enum {
    DB_OK = 0,
    DB_ERR_INVALID_ARG,
    DB_ERR_CONN,
    DB_ERR_QUERY,
    DB_ERR_NOT_FOUND,
    DB_ERR_ALREADY_EXISTS,
    DB_ERR_AUTH_FAILED,
    DB_ERR_FORBIDDEN
} db_status_t;

typedef struct {
    int64_t id;
    char name[DB_NAME_MAX];
    char created_at[DB_TIME_MAX];
    char last_login_at[DB_TIME_MAX];
} db_user_t;

typedef struct {
    int64_t id;
    char name[DB_NAME_MAX];
    char dir_path[DB_ENTRY_MAX];
    int64_t owner_id;
    char created_at[DB_TIME_MAX];
    char updated_at[DB_TIME_MAX];
} db_group_t;

typedef struct {
    int64_t id;
    int64_t requester_id;
    char requester_name[DB_NAME_MAX];
    int64_t group_id;
    char message[DB_MSG_MAX];
    char status[DB_STATUS_MAX];
    char created_at[DB_TIME_MAX];
} db_request_t;

typedef struct {
    int64_t id;
    char name[DB_ENTRY_MAX];
    char type[DB_TYPE_MAX]; /* "FILE" hoac "DIRECTORY" */
    char path[DB_PATH_MAX];
    int64_t uploader_id;
    int64_t group_id;
    int64_t parent_id;      /* 0 neu nam o thu muc goc (NULL trong DB) */
    char uploaded_at[DB_TIME_MAX];
} db_entry_t;

/* ===================== 0. Ket noi & Tien ich ===================== */

PGconn *db_connect(const char *conninfo);
void db_disconnect(PGconn *conn);
db_status_t db_exec_sql_file(PGconn *conn, const char *filepath);
const char *db_status_str(db_status_t status);

/* ===================== 1. Dang ky & Dang nhap ===================== */

db_status_t db_user_exists(PGconn *conn, const char *username, bool *out_exists);

db_status_t db_user_register(PGconn *conn,
                             const char *username,
                             const char *plain_password,
                             db_user_t *out_user);

db_status_t db_user_login(PGconn *conn,
                          const char *username,
                          const char *plain_password,
                          db_user_t *out_user);

/* ===================== 2. Quan ly nhom chia se ===================== */

/**
 * Tao nhom moi, dat dir_path = 'root/<id>' va tu dong them owner vao users_groups.
 */
db_status_t db_group_create(PGconn *conn,
                            const char *group_name,
                            int64_t owner_id,
                            db_group_t *out_group);

db_status_t db_group_is_owner(PGconn *conn,
                              int64_t group_id,
                              int64_t user_id,
                              bool *out_is_owner);

db_status_t db_group_is_member(PGconn *conn,
                               int64_t group_id,
                               int64_t user_id,
                               bool *out_is_member);

/**
 * Lay danh sach nhom ma user dang la thanh vien.
 * Nguoi goi giai phong *out_groups bang free().
 */
db_status_t db_group_list_by_user(PGconn *conn,
                                  int64_t user_id,
                                  db_group_t **out_groups,
                                  size_t *out_count);

/**
 * Lay danh sach tat ca cac nhom tren he thong (de user tim va xin tham gia).
 * Nguoi goi giai phong *out_groups bang free().
 */
db_status_t db_group_list_all(PGconn *conn,
                              db_group_t **out_groups,
                              size_t *out_count);

/**
 * Lay danh sach thanh vien trong nhom (yeu cau caller_id la thanh vien nhom).
 * Nguoi goi giai phong *out_users bang free().
 */
db_status_t db_group_list_members(PGconn *conn,
                                  int64_t group_id,
                                  int64_t caller_id,
                                  db_user_t **out_users,
                                  size_t *out_count);

/* ===================== 3. Yeu cau tham gia nhom ===================== */

/**
 * Gui yeu cau xin tham gia nhom.
 * Tra ve DB_ERR_ALREADY_EXISTS neu user da trong nhom hoac da co request PENDING.
 */
db_status_t db_request_create(PGconn *conn,
                              int64_t group_id,
                              int64_t requester_id,
                              const char *message,
                              db_request_t *out_req);

/**
 * Lay danh sach cac request PENDING cua nhom (chi truong nhom leader_id duoc xem).
 * Nguoi goi giai phong *out_reqs bang free().
 */
db_status_t db_request_list_pending(PGconn *conn,
                                    int64_t group_id,
                                    int64_t leader_id,
                                    db_request_t **out_reqs,
                                    size_t *out_count);

/**
 * Truong nhom duyet yeu cau (accept = true -> ACCEPTED va them vao users_groups;
 * accept = false -> REJECTED).
 * Tra ve DB_ERR_FORBIDDEN neu leader_id khong phai owner cua nhom.
 */
db_status_t db_request_respond(PGconn *conn,
                               int64_t request_id,
                               int64_t leader_id,
                               bool accept);

/* ===================== 4. Quan ly File & Thu muc ===================== */

/**
 * Kiem tra trong cung group_id va parent_id (0 = root) da co entry trung ten chua.
 */
db_status_t db_entry_exists_name(PGconn *conn,
                                 int64_t group_id,
                                 int64_t parent_id,
                                 const char *name,
                                 bool *out_exists);

/**
 * Tao entry moi (type = "FILE" khi upload file, "DIRECTORY" khi tao thu muc con).
 * Yeu cau uploader_id phai la thanh vien trong nhom.
 * parent_id = 0 neu tao ngay tai thu muc goc cua nhom (root/<group_id>/<name>).
 */
db_status_t db_entry_create(PGconn *conn,
                            int64_t group_id,
                            int64_t parent_id,
                            int64_t uploader_id,
                            const char *name,
                            const char *type,
                            db_entry_t *out_entry);

/**
 * Lay thong tin chi tiet (bao gom path) cua 1 entry (dung khi download file).
 * Yeu cau caller_id la thanh vien trong nhom.
 */
db_status_t db_entry_get_by_id(PGconn *conn,
                               int64_t group_id,
                               int64_t entry_id,
                               int64_t caller_id,
                               db_entry_t *out_entry);

/**
 * Lay danh sach cac file va thu muc con truc tiep duoi parent_id (0 = root cua nhom).
 * Nguoi goi giai phong *out_entries bang free().
 */
db_status_t db_entry_list_children(PGconn *conn,
                                   int64_t group_id,
                                   int64_t parent_id,
                                   int64_t caller_id,
                                   db_entry_t **out_entries,
                                   size_t *out_count);

/**
 * Quay nguoc tro lai tang cha cua thu muc hien tai (current_dir_id).
 * Tra ve danh sach cac entry cung tang voi current_dir_id va luu parent_id moi vao *out_new_parent_id.
 */
db_status_t db_entry_list_parent_layer(PGconn *conn,
                                       int64_t group_id,
                                       int64_t current_dir_id,
                                       int64_t caller_id,
                                       int64_t *out_new_parent_id,
                                       db_entry_t **out_entries,
                                       size_t *out_count);

/**
 * Doi ten file hoac thu muc con (CHI TRUONG NHOM leader_id co quyen).
 * Neu entry la DIRECTORY, tu dong cap nhat path cua toan bo cac entry con/chau ben duoi.
 */
db_status_t db_entry_rename(PGconn *conn,
                            int64_t group_id,
                            int64_t entry_id,
                            int64_t leader_id,
                            const char *new_name,
                            db_entry_t *out_entry);

/**
 * Xoa file hoac thu muc con (CHI TRUONG NHOM leader_id co quyen).
 * Xoa de quy toan bo entry con/chau qua ON DELETE CASCADE va tra ve thong tin entry goc bi xoa
 * (bao gom path) de server xoa file/thu muc vat ly tren o cung.
 */
db_status_t db_entry_delete(PGconn *conn,
                            int64_t group_id,
                            int64_t entry_id,
                            int64_t leader_id,
                            db_entry_t *out_deleted);

#endif /* FILESHARE_DB_H */
