#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/server/db/db.h"
#include "../../src/server/db/password.h"

static void test_password_hashing(void)
{
    char h1[PASSWORD_HASH_MAX_LEN];
    char h2[PASSWORD_HASH_MAX_LEN];

    assert(password_hash("MatKhauBiMat123!", h1, sizeof(h1)) == 0);
    assert(password_hash("MatKhauBiMat123!", h2, sizeof(h2)) == 0);

    /* Hai lan bam cung 1 mat khau phai sinh ra 2 chuoi khac nhau do salt ngau nhien */
    assert(strcmp(h1, h2) != 0);

    /* Xac thuc mat khau dung */
    assert(password_verify("MatKhauBiMat123!", h1) == true);
    assert(password_verify("MatKhauBiMat123!", h2) == true);

    /* Xac thuc mat khau sai */
    assert(password_verify("SaiMatKhau", h1) == false);
    assert(password_verify("", h1) == false);
    assert(password_verify(NULL, h1) == false);
    assert(password_verify("MatKhauBiMat123!", "invalid_hash_format") == false);

    /* Tham so khong hop le */
    assert(password_hash("", h1, sizeof(h1)) == -1);
    assert(password_hash(NULL, h1, sizeof(h1)) == -1);
    assert(password_hash("abc", h1, 10) == -1);
}

static void test_validators(void)
{
    assert(db_is_valid_username("alice_01") == true);
    assert(db_is_valid_username("bob-2.0") == true);
    assert(db_is_valid_username("") == false);
    assert(db_is_valid_username(NULL) == false);
    assert(db_is_valid_username("user with space") == false);
    assert(db_is_valid_username("user/slash") == false);

    assert(db_is_valid_entry_name("report_2026.pdf") == true);
    assert(db_is_valid_entry_name("thu_muc_con") == true);
    assert(db_is_valid_entry_name(".") == false);
    assert(db_is_valid_entry_name("..") == false);
    assert(db_is_valid_entry_name("../etc/passwd") == false);
    assert(db_is_valid_entry_name("a/b") == false);
    assert(db_is_valid_entry_name("a\\b") == false);
    assert(db_is_valid_entry_name("") == false);
    assert(db_is_valid_entry_name(NULL) == false);
}

static void test_null_connection_guards(void)
{
    bool flag = false;
    db_user_t user;
    db_group_t group;
    db_request_t req;
    db_entry_t entry;

    assert(db_connect(NULL) == NULL);
    db_disconnect(NULL);
    assert(strcmp(db_status_str(DB_OK), "OK") == 0);
    assert(strcmp(db_status_str(DB_ERR_FORBIDDEN), "Khong co quyen thuc hien thao tac nay") == 0);

    assert(db_user_exists(NULL, "alice", &flag) == DB_ERR_CONN);
    assert(db_user_register(NULL, "alice", "pass123", &user) == DB_ERR_CONN);
    assert(db_user_login(NULL, "alice", "pass123", &user) == DB_ERR_CONN);
    assert(db_group_create(NULL, "group1", 1, &group) == DB_ERR_CONN);
    assert(db_group_is_owner(NULL, 1, 1, &flag) == DB_ERR_CONN);
    assert(db_group_is_member(NULL, 1, 1, &flag) == DB_ERR_CONN);
    assert(db_request_create(NULL, 1, 2, "join", &req) == DB_ERR_CONN);
    assert(db_request_respond(NULL, 1, 1, true) == DB_ERR_CONN);
    assert(db_entry_create(NULL, 1, 0, 1, "a.txt", "FILE", &entry) == DB_ERR_CONN);
    assert(db_entry_rename(NULL, 1, 1, 1, "b.txt", &entry) == DB_ERR_CONN);
    assert(db_entry_delete(NULL, 1, 1, 1, &entry) == DB_ERR_CONN);
}

static void test_live_postgres_if_configured(void)
{
    const char *dsn = getenv("TEST_DATABASE_URL");
    if (dsn == NULL || dsn[0] == '\0') {
        return;
    }

    PGconn *conn = db_connect(dsn);
    assert(conn != NULL);

    /* Khoi tao schema */
    assert(db_exec_sql_file(conn, "db/migrations/001_init.sql") == DB_OK);
    PQclear(PQexec(conn, "TRUNCATE users, groups, users_groups, entries, requests RESTART IDENTITY CASCADE;"));

    /* 1. Dang ky & Dang nhap */
    db_user_t leader, member, outsider;
    assert(db_user_register(conn, "leader_user", "secret123", &leader) == DB_OK);
    assert(db_user_register(conn, "leader_user", "secret123", NULL) == DB_ERR_ALREADY_EXISTS);
    assert(db_user_register(conn, "member_user", "memberpass", &member) == DB_OK);
    assert(db_user_register(conn, "outsider_user", "outpass", &outsider) == DB_OK);

    db_user_t login_u;
    assert(db_user_login(conn, "leader_user", "wrongpass", &login_u) == DB_ERR_AUTH_FAILED);
    assert(db_user_login(conn, "leader_user", "secret123", &login_u) == DB_OK);
    assert(login_u.id == leader.id);

    /* 2. Tao nhom (leader tu dong la owner va thanh vien) */
    db_group_t grp;
    assert(db_group_create(conn, "Nhom_Lap_Trinh_Mang", leader.id, &grp) == DB_OK);
    bool is_owner = false, is_member = false;
    assert(db_group_is_owner(conn, grp.id, leader.id, &is_owner) == DB_OK && is_owner);
    assert(db_group_is_member(conn, grp.id, leader.id, &is_member) == DB_OK && is_member);

    /* 3. Xin tham gia nhom & Truong nhom duyet */
    db_request_t req;
    assert(db_request_create(conn, grp.id, member.id, "Cho minh vao nhom", &req) == DB_OK);
    assert(db_request_create(conn, grp.id, member.id, "Spam request", NULL) == DB_ERR_ALREADY_EXISTS);

    /* Nguoi khong phai truong nhom khong duoc duyet */
    assert(db_request_respond(conn, req.id, outsider.id, true) == DB_ERR_FORBIDDEN);
    /* Truong nhom duyet thanh cong */
    assert(db_request_respond(conn, req.id, leader.id, true) == DB_OK);
    assert(db_group_is_member(conn, grp.id, member.id, &is_member) == DB_OK && is_member);

    /* 4. Thanh vien tao thu muc con va upload file */
    db_entry_t dir_docs, file_slide;
    assert(db_entry_create(conn, grp.id, 0, member.id, "docs", "DIRECTORY", &dir_docs) == DB_OK);
    assert(db_entry_create(conn, grp.id, dir_docs.id, member.id, "slide.pdf", "FILE", &file_slide) == DB_OK);
    /* Nguoi ngoai nhom khong duoc upload */
    assert(db_entry_create(conn, grp.id, 0, outsider.id, "hack.txt", "FILE", NULL) == DB_ERR_FORBIDDEN);

    /* 5. Chi truong nhom duoc doi ten va xoa */
    db_entry_t renamed_dir, check_child;
    assert(db_entry_rename(conn, grp.id, dir_docs.id, member.id, "tai_lieu", &renamed_dir) == DB_ERR_FORBIDDEN);
    assert(db_entry_rename(conn, grp.id, dir_docs.id, leader.id, "tai_lieu", &renamed_dir) == DB_OK);

    /* Kiem tra path cua file con ben trong da tu dong cap nhat theo ten thu muc moi */
    assert(db_entry_get_by_id(conn, grp.id, file_slide.id, member.id, &check_child) == DB_OK);
    assert(strstr(check_child.path, "/tai_lieu/slide.pdf") != NULL);

    /* Xoa thu muc: thanh vien bi tu choi, truong nhom xoa thanh cong */
    assert(db_entry_delete(conn, grp.id, dir_docs.id, member.id, NULL) == DB_ERR_FORBIDDEN);
    assert(db_entry_delete(conn, grp.id, dir_docs.id, leader.id, NULL) == DB_OK);
    assert(db_entry_get_by_id(conn, grp.id, file_slide.id, leader.id, &check_child) == DB_ERR_NOT_FOUND);

    db_disconnect(conn);
    printf("PASS: live PostgreSQL integration tests\n");
}

int main(void)
{
    test_password_hashing();
    test_validators();
    test_null_connection_guards();
    test_live_postgres_if_configured();
    puts("PASS: db unit tests");
    return 0;
}
