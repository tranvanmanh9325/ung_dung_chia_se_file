# Thiết kế CSDL cho ứng dụng chia sẻ file (PostgreSQL 16.0 hoặc mới hơn)

## 1. Danh sách các bảng

### 1.1. `users`

Bảng này chứa thông tin người dùng, phục vụ cho việc đăng nhập, đăng ký.
Cột `name` không được trùng lặp tên user. Cột `password` lưu chuỗi băm kèm salt (`$s256$<salt>$<hash>`).

```sql
CREATE TABLE users (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(64) UNIQUE NOT NULL,
    password VARCHAR(255) NOT NULL,

    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_login_at TIMESTAMPTZ
);
```

### 1.2. `groups`

Bảng này chứa các nhóm phục vụ chia sẻ file. Người tạo nhóm (`owner_id`) có quyền trưởng nhóm.

```sql
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
```

### 1.3. `users_groups`

Bảng này liên kết 2 bảng `users` và `groups` theo quan hệ Many-to-Many (danh sách thành viên của nhóm). Khi tạo nhóm mới, trưởng nhóm (`owner_id`) được tự động thêm vào bảng này.

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

### 1.4. `entries`

Bảng này chứa toàn bộ các file và thư mục con (directory index), phân biệt thông qua cột `type` (`'FILE'` hoặc `'DIRECTORY'`).

- Mỗi entry nằm ở thư mục gốc của nhóm sẽ có `parent_id IS NULL`.
- Mỗi entry nằm trong thư mục con sẽ có `parent_id` trỏ đến `id` của một entry có `type = 'DIRECTORY'` trong cùng nhóm.
- Khi upload file hoặc tạo thư mục con: Bất kỳ thành viên nào trong nhóm (`users_groups`) đều có quyền thực hiện.
- Khi download file: Thành viên trong nhóm trỏ đến `id` của entry thuộc type `'FILE'`, server lấy `entries.path` để đọc file vật lý.
- Khi đổi tên (rename) hoặc xóa (delete) file/thư mục: **Chỉ trưởng nhóm (`groups.owner_id`)** mới có quyền thực hiện. Khi xóa một thư mục, toàn bộ các entry con/cháu bên trong sẽ tự động bị xóa theo cơ chế `ON DELETE CASCADE`. Khi đổi tên một thư mục, đường dẫn `path` của thư mục đó và toàn bộ các entry con/cháu bên trong được cập nhật đồng bộ trong cùng một transaction.

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
```

### 1.5. `requests`

Danh sách các yêu cầu xin tham gia nhóm, chỉ trưởng nhóm (`owner_id`) mới có quyền xem và phê duyệt (`ACCEPTED` / `REJECTED`).

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

CREATE UNIQUE INDEX idx_requests_unique_pending
    ON requests (requester_id, group_id)
    WHERE status = 'PENDING';
```

---

## 2. Danh sách các lệnh truy vấn

### 2.1. Truy vấn phục vụ đăng nhập, đăng ký

#### 1. Kiểm tra username có tồn tại không

```sql
SELECT EXISTS (
    SELECT 1
    FROM users
    WHERE name = $1
);
```

- `$1`: Username cần kiểm tra.

#### 2. Đăng ký tài khoản mới

```sql
INSERT INTO users (name, password)
VALUES ($1, $2)
RETURNING id, name, created_at, last_login_at;
```

- `$1`: Username.
- `$2`: Chuỗi mật khẩu đã băm kèm salt.

#### 3. Lấy thông tin xác thực khi đăng nhập và cập nhật `last_login_at`

```sql
SELECT id, name, password, created_at, last_login_at
FROM users
WHERE name = $1;
```

Sau khi tầng C xác thực mật khẩu khớp với chuỗi băm:

```sql
UPDATE users
SET last_login_at = CURRENT_TIMESTAMP
WHERE id = $1
RETURNING last_login_at;
```

---

### 2.2. Truy vấn phục vụ quản lý nhóm chia sẻ file

#### 1. Lấy toàn bộ danh sách thành viên của nhóm

```sql
SELECT id, name, last_login_at FROM users
JOIN users_groups
    ON users.id = users_groups.user_id
WHERE users_groups.group_id = $1;
```

-```$1```: Placeholder cho ID của nhóm chia sẻ từ bảng ```groups```.

#### 2. Lấy toàn bộ các nhóm mà người dùng đang ở trong (hiển thị cho người dùng đó)

```sql
SELECT groups.* FROM groups
JOIN users_groups
    ON groups.id = users_groups.group_id
WHERE users_groups.user_id = $1;
```

-```$1```: Placeholder cho ID của người dùng từ bảng ```users```.

#### 3. Lấy toàn bộ các yêu cầu tham gia nhóm (chỉ hiển thị với người tạo nhóm)

**CHÚ Ý**: Chỉ hiển thị các yêu cầu đang có trạng thái ```PENDING```.

```sql
SELECT * FROM requests
WHERE group_id = $1
    AND status = 'PENDING';
```

-```$1```: Placeholder cho ID của nhóm chia sẻ từ bảng ```groups```.

### 2.3. Truy vấn phục vụ hệ thống file
#### 1. Lấy toàn bộ các file và tệp con ở dưới một tệp

Do ứng dụng sẽ hiển thị từng layer một thay vì hiển thị toàn bộ cây file, chỉ cần chạy 1 hàm truy vấn đơn giản.

Ta chỉ cần chạy lại hàm này khi truy vấn tệp con ở độ sâu tiếp theo.

```sql
SELECT * FROM entries
WHERE group_id = $1
    AND parent_id = $2;
```

-```$1```: Placeholder cho ID của nhóm chia sẻ từ bảng ```groups```.
-```$2```: Placeholder cho ID của tệp cha từ chính bảng ```entries```.


#### 2. Quay ngược trở lại tệp cha (chỉ hiển thị lựa chọn này nếu tệp hiện tại không phải tệp root của nhóm)

```sql
SELECT * FROM entries
WHERE group_id = $2
    AND parent_id IS NOT DISTINCT FROM (
        SELECT parent_id
        FROM entries
        WHERE id = $1
            AND group_id = $2
    );
```

-```$1```: Placeholder cho ID của tệp mà người dùng đang nhìn thông qua bảng ```entries```.
-```$2```: Placeholder cho ID của nhóm chia sẻ từ bảng ```groups```.

#### 3. Kiểm tra trùng tên file (không cho phép hai tệp hoặc file có trùng tên)

Hai file không được trùng tên, hai tệp không được trùng tên, 1 file và 1 tệp có thể trùng tên.

```sql
SELECT EXISTS (
    SELECT 1
    FROM entries
    WHERE group_id = $1
        AND parent_id IS NOT DISTINCT FROM $2
        AND name = $3
        AND type = $4
);
```

- ```$1```: Placeholder cho biến ID của nhóm từ bảng ```groups```.
- ```$2```: Placeholder cho biến ID của tệp cha của entry.
- ```$3```: Placeholder cho biến tên của file hoặc tệp con
- ```$4```: Placeholder cho biến loại entry (file hoặc directory)

#### 4. Tạo nhóm mới

Tạo nhóm mới và thực hiện ```INSERT``` vào bảng ```groups```.

```sql
WITH new_group AS (
    INSERT INTO groups (name, dir_path, owner_id)
    VALUES ($1, '', $2)
    RETURNING id
),
updated_group AS (
    UPDATE groups
    SET dir_path = 'root/' || id
    WHERE id = (SELECT id FROM new_group)
    RETURNING id, name, dir_path, owner_id, created_at, updated_at
),
owner_membership AS (
    INSERT INTO users_groups (user_id, group_id)
    SELECT owner_id, id FROM updated_group
)
SELECT id, name, dir_path, owner_id, created_at, updated_at
FROM updated_group;
```

- `$1`: Tên nhóm muốn tạo.
- `$2`: ID người tạo nhóm (`owner_id`).

#### 2. Kiểm tra quyền trưởng nhóm (`owner_id`) và quyền thành viên

```sql
-- Kiểm tra trưởng nhóm
SELECT EXISTS (
    SELECT 1 FROM groups
    WHERE id = $1 AND owner_id = $2
);

-- Kiểm tra thành viên trong nhóm
SELECT EXISTS (
    SELECT 1 FROM users_groups
    WHERE group_id = $1 AND user_id = $2
);
```

#### 3. Lấy toàn bộ các thành viên trong nhóm (không trả về cột `password`)

```sql
SELECT users.id, users.name, users.created_at, users.last_login_at
FROM users
JOIN users_groups
    ON users.id = users_groups.user_id
WHERE users_groups.group_id = $1
ORDER BY users.id ASC;
```

#### 4. Lấy các nhóm mà người dùng đang là thành viên

```sql
SELECT groups.id, groups.name, groups.dir_path, groups.owner_id,
       groups.created_at, groups.updated_at
FROM groups
JOIN users_groups
    ON groups.id = users_groups.group_id
WHERE users_groups.user_id = $1
ORDER BY groups.id ASC;
```

#### 5. Gửi yêu cầu xin tham gia nhóm và duyệt yêu cầu (dành cho trưởng nhóm)

```sql
-- Gửi yêu cầu tham gia
INSERT INTO requests (requester_id, group_id, message)
VALUES ($1, $2, $3)
RETURNING id, requester_id, group_id, message, status, created_at;

-- Xem danh sách yêu cầu đang chờ duyệt (kèm tên người gửi)
SELECT r.id, r.requester_id, u.name AS requester_name,
       r.group_id, r.message, r.status, r.created_at
FROM requests r
JOIN users u ON u.id = r.requester_id
WHERE r.group_id = $1
  AND r.status = 'PENDING'
ORDER BY r.created_at ASC;

-- Trưởng nhóm duyệt (ACCEPTED hoặc REJECTED) và thêm vào users_groups nếu ACCEPTED
UPDATE requests
SET status = $2,
WHERE id = $1 AND status = 'PENDING'
RETURNING requester_id, group_id;

INSERT INTO users_groups (user_id, group_id)
VALUES ($1, $2)
ON CONFLICT DO NOTHING;
```

---

### 2.3. Truy vấn phục vụ hệ thống file

#### 1. Lấy danh sách file và thư mục con ở một tầng (layer)

```sql
SELECT id, name, type, path, uploader_id, group_id, parent_id, uploaded_at
FROM entries
WHERE group_id = $1
  AND parent_id IS NOT DISTINCT FROM $2
ORDER BY type ASC, name ASC;
```

- `$1`: ID của nhóm chia sẻ.
- `$2`: ID của thư mục cha (`NULL` nếu đang xem ở thư mục gốc của nhóm).

#### 2. Quay ngược trở lại thư mục cha

```sql
SELECT id, name, type, path, uploader_id, group_id, parent_id, uploaded_at
FROM entries
WHERE group_id = $2
  AND parent_id IS NOT DISTINCT FROM (
      SELECT parent_id
      FROM entries
      WHERE id = $1
        AND group_id = $2
  )
ORDER BY type ASC, name ASC;
```

#### 3. Kiểm tra trùng tên trong cùng một thư mục cha

Trong cùng một thư mục cha của một nhóm, không cho phép hai entry (bất kể `FILE` hay `DIRECTORY`) trùng tên:

```sql
SELECT EXISTS (
    SELECT 1
    FROM entries
    WHERE group_id = $1
      AND parent_id IS NOT DISTINCT FROM $2
      AND name = $3
);
```

#### 4. Tạo entry mới (Upload file hoặc tạo thư mục con)

- **Trường hợp 1**: Tạo ngay tại thư mục gốc của nhóm (`parent_id IS NULL`):

```sql
INSERT INTO entries (name, type, path, uploader_id, group_id, parent_id)
SELECT $1, $2, g.dir_path || '/' || $1, $3, g.id, NULL
FROM groups g
WHERE g.id = $4
RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;
```

- **Trường hợp 2**: Tạo bên trong một thư mục con (`parent_id = $5`):

```sql
INSERT INTO entries (name, type, path, uploader_id, group_id, parent_id)
SELECT $1, $2, parent.path || '/' || $1, $3, parent.group_id, parent.id
FROM entries AS parent
WHERE parent.id = $5
  AND parent.group_id = $4
  AND parent.type = 'DIRECTORY'
RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;
```

#### 5. Đổi tên file hoặc thư mục con (Chỉ trưởng nhóm)

Thực hiện trong một Transaction sau khi kiểm tra người gọi là `owner_id` của nhóm:

```sql
-- Bước 1: Cập nhật tên và đường dẫn của chính entry đó
UPDATE entries
SET name = $1,
    path = $2
WHERE id = $3 AND group_id = $4
RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;

-- Bước 2: Nếu entry là DIRECTORY, cập nhật tiền tố đường dẫn của toàn bộ entry con/cháu
UPDATE entries
SET path = $1 || SUBSTRING(path FROM CHAR_LENGTH($2) + 1)
WHERE group_id = $3
  AND LEFT(path, CHAR_LENGTH($2) + 1) = ($2 || '/');
```

#### 6. Xóa file hoặc thư mục con (Chỉ trưởng nhóm)

```sql
DELETE FROM entries
WHERE id = $1 AND group_id = $2
RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;
```

---

## 3. Quy ước chung

### 3.1. Đường dẫn file
- Sử dụng một thư mục cha chung đặt tên là `root/` cho toàn server.
- Mỗi nhóm có một thư mục riêng đặt ngay dưới `root/`, định danh bằng `id` của nhóm: `root/1`, `root/2`, ... (tránh xung đột khi hai nhóm trùng tên).
- Không truyền trực tiếp đường dẫn vật lý từ client khi upload/download/delete/rename; client truyền `entry_id` và `group_id`, server kiểm tra quyền và tra cứu `entries.path` trong CSDL.

### 3.2. Quy ước trong cơ sở dữ liệu
- Một entry nằm ở chính **thư mục gốc của nhóm** nếu `parent_id` có giá trị là `NULL` (trong cấu trúc C `db_entry_t`, giá trị `parent_id = 0` đại diện cho `NULL`).
- Ví dụ: Nếu file `text.txt` nằm ở `root/1/text.txt` thì `parent_id` của entry ứng với nó là `NULL`.
