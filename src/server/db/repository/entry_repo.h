#ifndef ENTRY_REPO_H
#define ENTRY_REPO_H

#include <stdbool.h>
#include <libpq-fe.h>
#include "models.h"

static const char* QUERY_GET_ENTRIES_BY_PARENT =
    "SELECT id, name, type, path, uploader_id, group_id, parent_id, uploaded_at "
    "FROM entries "
    "WHERE group_id = $1 "
    "AND parent_id IS NOT DISTINCT FROM $2 "
    "ORDER BY type ASC, name ASC;";

static const char* QUERY_GET_PARENT_DIRECTORY =
    "SELECT id, name, type, path, uploader_id, group_id, parent_id, uploaded_at "
    "FROM entries "
    "WHERE group_id = $2 "
    "AND parent_id IS NOT DISTINCT FROM ("
        "SELECT parent_id "
        "FROM entries "
        "WHERE id = $1 AND group_id = $2"
    ") "
    "ORDER BY type ASC, name ASC;";

static const char* QUERY_CHECK_DUPLICATE_ENTRY =
    "SELECT EXISTS ("
        "SELECT 1 FROM entries "
        "WHERE group_id = $1 "
        "AND parent_id IS NOT DISTINCT FROM $2 "
        "AND name = $3 "
        "AND type = $4"
    ");";

static const char* QUERY_INSERT_ENTRY =
    "INSERT INTO entries (name, type, path, uploader_id, group_id, parent_id) "
    "SELECT $1, $2, "
        "CASE "
            "WHEN $3 IS NULL THEN g.dir_path || '/' || $1 "
            "ELSE parent.path || '/' || $1 "
        "END, "
        "$4, g.id, $3 "
    "FROM groups g "
    "LEFT JOIN entries parent "
        "ON parent.id = $3 "
        "AND parent.group_id = g.id "
        "AND parent.type = 'DIRECTORY' "
    "WHERE g.id = $5 "
        "AND ($3 IS NULL OR parent.id IS NOT NULL) "
    "RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;";

static const char* QUERY_RENAME_ENTRY =
    "UPDATE entries "
    "SET name = $1, path = $2 "
    "WHERE id = $3 AND group_id = $4 "
    "RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;";

static const char* QUERY_UPDATE_DESCENDANT_PATHS =
    "UPDATE entries "
    "SET path = $1 || SUBSTRING(path FROM CHAR_LENGTH($2) + 1) "
    "WHERE group_id = $3 "
    "AND LEFT(path, CHAR_LENGTH($2) + 1) = ($2 || '/');";

static const char* QUERY_DELETE_ENTRY =
    "DELETE FROM entries "
    "WHERE id = $1 AND group_id = $2 "
    "RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;";

void get_entries_by_parent(PGconn* conn, long group_id, const char* parent_id, Entry** entries, int* count);
void get_parent_dir_entries(PGconn* conn, long entry_id, long group_id, Entry** entries, int* count);
void check_duplicate_entry(PGconn* conn, long group_id, const char* parent_id, const char* name, const char* type, bool* res);
void insert_entry(PGconn* conn, const char* name, const char* type, const char* parent_id, long uploader_id, long group_id, Entry** entry);
void rename_entry(PGconn* conn, long entry_id, long group_id, const char* new_name, const char* new_path);
void update_descendant_paths(PGconn* conn, const char* new_path, const char* old_path, long group_id);
void delete_entry(PGconn* conn, long entry_id, long group_id, Entry** deleted_entry);

#endif