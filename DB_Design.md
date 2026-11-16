# Thiết kế CSDL cho ứng dụng chia sẻ file

## 1. Thông tin chung

- PostgreSQL (> 16.0)

## 2. Danh sách các bảng

### 2.1. ```users```

Bảng này sẽ chứa thông tin người dùng, phục vụ cho việc đăng nhập, đăng ký

```sql
CREATE TABLE users (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    password VARCHAR(255) NOT NULL,

    created_at TIMESTAMPZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_login_at TIMESTAMPZ
);
```

### 2.2. ```groups```

Bảng này chứa các nhóm phục vụ chia sẻ file

```sql
CREATE TABLE groups (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    dir_path VARCHAR(255) NOT NULL,
    owner_id BIGINT NOT NULL,

    created_at TIMESTAMPZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    FOREIGN KEY (owner_id)
        REFERENCES users(id)
);
```

### 2.3. ```users_groups```

Bảng này liên kết 2 bảng ```users``` và ```groups``` theo quan hệ Many-to-Many.

```sql
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
```

### 2.4. ```entries```

Bảng này chứa toàn bộ các file và directory index, phân biệt thông qua cột ```type```. 

Mỗi entry chứa cột ```parent_id``` trỏ đến entry khác trong cùng bảng, ```parent_id``` bắt buộc phải trỏ đến một record có type là ```DIRECTORY```.

Khi upload file, người dùng sẽ trỏ đến ```id``` của một entry thuộc type ```DIRECTORY```  để upload tới. Đường dẫn của file mới này sẽ sử dụng đường dẫn đã có của parent và nối với tên file chuẩn bị upload.

Khi download file, người dùng trỏ đến ```id``` của entry thuộc type ```FILE``` để tải file. Máy chủ lấy ```id``` của request, resolve đường dẫn của file thông qua cột ```entries.path``` để tìm file và thực hiện tải xuống.

Khi delete file, người dùng trỏ đến ```id``` của entry thuộc type bất kì, và toàn bộ các entry reference đến id đó (trực tiếp hoặc gián tiếp) sẽ bị xoá cùng với các entry con của nó trong cùng một group.

```sql
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
        REFERENCES users(id),

    FOREIGN KEY (group_id)
        REFERENCES groups(id)
        ON DELETE CASCADE,

    FOREIGN KEY (parent_id, group_id)
        REFERENCES entries(id, group_id)
        ON DELETE CASCADE
);
```

### 2.5. ```requests```

Danh sách các request tham gia một nhóm, chỉ hiển thị với group leader.

```sql
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
```