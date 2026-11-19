CREATE TABLE users (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(64) UNIQUE NOT NULL,
    password VARCHAR(255) NOT NULL,

    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_login_at TIMESTAMPTZ
);

CREATE TABLE groups (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    dir_path VARCHAR(255) NOT NULL,
    owner_id BIGINT NOT NULL,

    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    FOREIGN KEY (owner_id)
        REFERENCES users(id)
        ON DELETE CASCADE
);

CREATE TABLE users_groups (
    user_id BIGINT NOT NULL,
    group_id BIGINT NOT NULL,

    PRIMARY KEY (user_id, group_id),

    FOREIGN KEY (user_id)
        REFERENCES users(id)
        ON DELETE CASCADE,

    FOREIGN KEY (group_id)
        REFERENCES groups(id)
        ON DELETE CASCADE
);

CREATE TABLE entries (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    type VARCHAR(16) NOT NULL CHECK (type IN ('FILE', 'DIRECTORY')),
    path VARCHAR(1024) NOT NULL,

    uploader_id BIGINT NOT NULL,
    group_id BIGINT NOT NULL,
    parent_id BIGINT,

    uploaded_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (id, group_id),
    UNIQUE (group_id, path),

    FOREIGN KEY (uploader_id)
        REFERENCES users(id)
        ON DELETE CASCADE,

    FOREIGN KEY (group_id)
        REFERENCES groups(id)
        ON DELETE CASCADE,

    FOREIGN KEY (parent_id, group_id)
        REFERENCES entries(id, group_id)
        ON DELETE CASCADE
);

CREATE UNIQUE INDEX idx_entries_unique_name_in_parent
    ON entries (group_id, COALESCE(parent_id, 0), name);

CREATE TABLE requests (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,

    message VARCHAR(255) NOT NULL,

    requester_id BIGINT NOT NULL,
    group_id BIGINT NOT NULL,

    status VARCHAR(16) NOT NULL DEFAULT 'PENDING'
        CHECK (status IN ('PENDING', 'ACCEPTED', 'REJECTED')),

    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    FOREIGN KEY (requester_id)
        REFERENCES users(id)
        ON DELETE CASCADE,

    FOREIGN KEY (group_id)
        REFERENCES groups(id)
        ON DELETE CASCADE
);

CREATE UNIQUE INDEX idx_requests_unique_pending
    ON requests (requester_id, group_id)
    WHERE status = 'PENDING';