#ifndef USER_H
#define USER_H

// 1 UTF-8 character = 4 bytes
#define VARCHAR(n) ((n) * 4 + 1)

// User class
typedef struct {
    long id;
    char name[VARCHAR(64)];
    char password[VARCHAR(255)];

    char created_at[64];
    char last_login_at[64];
} User;


// Group class
typedef struct {
    long id;
    char name[VARCHAR(64)];
    char dir_path[VARCHAR(255)];
    long owner_id;

    char created_at[64];
    char updated_at[64];
} Group;

// Users + Groups link table
typedef struct {
    long id;
    char name[VARCHAR(255)];
    char type[VARCHAR(16)];
    char path[VARCHAR(1024)];

    long uploader_id;
    long group_id;
    long parent_id;

    char uploaded_at[64];
} Entry;

// Request table
typedef struct {
    long id;
    char message[VARCHAR(255)];
    
    long requester_id;
    long group_id;
    
    char status[VARCHAR(16)];

    char created_at[64];
} Request;

#endif