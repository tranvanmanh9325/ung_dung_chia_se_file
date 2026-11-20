#ifndef MODELS_H
#define MODELS_H

typedef struct {
    long id;
    char name[65];

    char created_at[65];
    char last_login_at[65];
} User;

typedef struct {
    long id;
    char name[65];
    char dir_path[256];
    long owner_id;          // FK to User.id

    char created_at[65];
    char updated_at[65];
} Group;

typedef struct {
    long id;
    char name[256];
    char type[17];
    char path[1025];

    long uploader_id;       // FK to User.id
    long group_id;          // FK to Group.id
    long parent_id;         // FK to Entry.id of the same group

    char uploaded_at[65];
} Entry;

typedef struct {
    long id;
    char message[256];
    
    long requester_id;      // FK to User.id
    long group_id;          // FK to Group.id

    char status[16];        // 'PENDING', 'ACCEPTED' or 'REJECTED'
    char created_at[65];
} Request;

#endif