# Thiết kế CSDL cho ứng dụng chia sẻ file (PostgreSQL 16.0 hoặc mới hơn)

## 1. Danh sách các bảng

### 1.1. `users`

Lưu thông tin người dùng phục vụ đăng nhập và đăng ký. `name` là duy nhất. `password` lưu chuỗi hash mật khẩu do libsodium tạo.

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

Lưu các nhóm chia sẻ file. Người tạo nhóm (`owner_id`) là trưởng nhóm.

```sql
CREATE TABLE groups (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    dir_path VARCHAR(255) NOT NULL,
    owner_id BIGINT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (owner_id) REFERENCES users(id) ON DELETE CASCADE
);
```

### 1.3. `users_groups`

Bảng liên kết Many-to-Many giữa `users` và `groups`. Khi tạo nhóm, trưởng nhóm cũng phải được thêm vào bảng này.

```sql
CREATE TABLE users_groups (
    user_id BIGINT NOT NULL,
    group_id BIGINT NOT NULL,
    PRIMARY KEY (user_id, group_id),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE
);
```

### 1.4. `entries`

Lưu thông tin metadata của file và thư mục. Cột `type` nhận một trong hai giá trị `FILE` hoặc `DIRECTORY`.

- Entry nằm trực tiếp trong thư mục gốc của nhóm có `parent_id IS NULL`.
- Entry nằm trong thư mục con có `parent_id` trỏ đến một entry kiểu `DIRECTORY` thuộc cùng nhóm.
- Thành viên nhóm có thể upload file hoặc tạo thư mục con.
- Khi download file, client gửi `entry_id`; server tra cứu `entries.path` thay vì nhận đường dẫn vật lý từ client.
- Theo quy tắc phân quyền của ứng dụng, chỉ trưởng nhóm được rename hoặc delete entry.
- Xóa một directory sẽ xóa toàn bộ entry con/cháu nhờ khóa ngoại tự tham chiếu với `ON DELETE CASCADE`.
- Khi rename directory, cần cập nhật `path` của directory và toàn bộ descendants trong cùng một transaction.

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
    FOREIGN KEY (uploader_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_id, group_id)
        REFERENCES entries(id, group_id)
        ON DELETE CASCADE
);

CREATE UNIQUE INDEX idx_entries_unique_name_in_parent
    ON entries (group_id, COALESCE(parent_id, 0), name, type);
```

Index trên đảm bảo không có hai entry cùng `group_id`, cùng thư mục cha, cùng `name` và cùng `type`. Do `type` nằm trong index, một file và một directory có thể có cùng tên trong cùng thư mục; hai file hoặc hai directory thì không thể trùng tên.

### 1.5. `requests`

Lưu yêu cầu tham gia nhóm. Trưởng nhóm có quyền xem và xử lý các yêu cầu của nhóm.

```sql
CREATE TABLE requests (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    message VARCHAR(255) NOT NULL,
    requester_id BIGINT NOT NULL,
    group_id BIGINT NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'PENDING'
        CHECK (status IN ('PENDING', 'ACCEPTED', 'REJECTED')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (requester_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE
);

CREATE UNIQUE INDEX idx_requests_unique_pending
    ON requests (requester_id, group_id)
    WHERE status = 'PENDING';
```

Index chỉ ngăn một người dùng có nhiều hơn một yêu cầu `PENDING` cho cùng một nhóm. Người dùng có thể gửi yêu cầu mới sau khi yêu cầu cũ đã được xử lý.

---

## 2. Các truy vấn

Các placeholder `$1`, `$2`, ... được truyền qua `PQexecParams()`; không nối trực tiếp dữ liệu đầu vào vào chuỗi SQL. Với tham số SQL `NULL` (ví dụ `parent_id` của entry ở thư mục gốc), truyền `NULL` trong mảng `params`, không truyền chuỗi `"NULL"`.

### 2.1. Truy vấn phục vụ đăng nhập và đăng ký

#### 1. Kiểm tra username có tồn tại không

```sql
SELECT EXISTS (
    SELECT 1
    FROM users
    WHERE name = $1
);
```

- `$1`: Username cần kiểm tra.
- Kết quả là một giá trị boolean (`t` hoặc `f` khi đọc qua libpq).

#### 2. Đăng ký tài khoản mới

```sql
INSERT INTO users (name, password)
VALUES ($1, $2)
RETURNING id, name, created_at;
```

- `$1`: Username.
- `$2`: Chuỗi hash mật khẩu.

Truy vấn trả về tối đa một dòng. Nếu insert thành công, `RETURNING` trả về một dòng; nếu vi phạm constraint, việc thực thi truy vấn thất bại.

#### 3. Lấy thông tin xác thực

```sql
SELECT name, password
FROM users
WHERE name = $1;
```

- `$1`: Username.
- Không có dòng trả về nghĩa là username không tồn tại.
- Tầng C dùng hash lấy được để xác thực mật khẩu bằng libsodium.

#### 4. Cập nhật thời điểm đăng nhập gần nhất

Sau khi xác thực mật khẩu thành công:

```sql
UPDATE users
SET last_login_at = CURRENT_TIMESTAMP
WHERE id = $1
RETURNING last_login_at;
```

- `$1`: ID người dùng.
- Truy vấn trả về tối đa một dòng.

### 2.2. Truy vấn phục vụ quản lý nhóm

#### 1. Lấy danh sách thành viên của một nhóm

```sql
SELECT id, name, last_login_at
FROM users
JOIN users_groups
    ON users.id = users_groups.user_id
WHERE users_groups.group_id = $1;
```

- `$1`: ID nhóm.
- Không lấy cột `password`.

#### 2. Lấy danh sách nhóm của một người dùng

```sql
SELECT groups.*
FROM groups
JOIN users_groups
    ON groups.id = users_groups.group_id
WHERE users_groups.user_id = $1;
```

- `$1`: ID người dùng.

#### 3. Lấy các yêu cầu đang chờ xử lý của một nhóm

```sql
SELECT *
FROM requests
WHERE group_id = $1
    AND status = 'PENDING';
```

- `$1`: ID nhóm.
- Chỉ trưởng nhóm được phép xem và xử lý các yêu cầu này; việc kiểm tra quyền cần được thực hiện ở tầng ứng dụng.

#### 4. Xử lý yêu cầu tham gia nhóm

```sql
WITH processed AS (
    UPDATE requests
    SET status = $1
    WHERE id = $2 AND status = 'PENDING'
    RETURNING requester_id, group_id
)
INSERT INTO users_groups (user_id, group_id)
SELECT requester_id, group_id
FROM processed
WHERE $1 = 'ACCEPTED'
RETURNING user_id;
```

- `$1`: Trạng thái xử lý, `ACCEPTED` hoặc `REJECTED`.
- `$2`: ID yêu cầu.
- Nếu `ACCEPTED`, yêu cầu được cập nhật và requester được thêm vào `users_groups`.
- Nếu `REJECTED`, yêu cầu được cập nhật nhưng không thêm thành viên.
- Nếu request không tồn tại hoặc không còn `PENDING`, CTE `processed` không có dòng nên không có thay đổi.
- Truy vấn này trả về `user_id` khi có thao tác thêm thành viên; với `REJECTED` hoặc request không hợp lệ, kết quả có zero rows. Vì vậy, `PQntuples(result) == 0` không đủ để phân biệt hai trường hợp đó.
- Việc kiểm tra người gọi là trưởng nhóm phải được thực hiện trước khi chạy truy vấn.

### 2.3. Truy vấn phục vụ hệ thống file

#### 1. Lấy danh sách entry trong một thư mục

```sql
SELECT id, name, type, path, uploader_id, group_id, parent_id, uploaded_at
FROM entries
WHERE group_id = $1
    AND parent_id IS NOT DISTINCT FROM $2
ORDER BY type ASC, name ASC;
```

- `$1`: ID nhóm.
- `$2`: ID thư mục cha; truyền SQL `NULL` khi đang xem thư mục gốc.
- `IS NOT DISTINCT FROM` cho phép so sánh đúng cả trường hợp hai giá trị đều là `NULL`.
- Kết quả có thể có zero hoặc nhiều dòng.

#### 2. Lấy danh sách entry trong thư mục cùng cấp với entry hiện tại

```sql
SELECT id, name, type, path, uploader_id, group_id, parent_id, uploaded_at
FROM entries
WHERE group_id = $2
    AND parent_id IS NOT DISTINCT FROM (
        SELECT parent_id
        FROM entries
        WHERE id = $1 AND group_id = $2
    )
ORDER BY type ASC, name ASC;
```

- `$1`: ID entry hiện tại.
- `$2`: ID nhóm.
- Truy vấn lấy các entry có cùng `parent_id` với entry hiện tại, tức danh sách trong thư mục chứa entry hiện tại.
- Lưu ý: nếu `$1` không tồn tại trong nhóm, scalar subquery trả về `NULL`, nên truy vấn có thể trả về các entry ở thư mục gốc. Tầng ứng dụng nên xác thực entry hiện tại trước khi dùng truy vấn này.

#### 3. Kiểm tra entry trùng trong cùng thư mục cha

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

- `$1`: ID nhóm.
- `$2`: ID thư mục cha; `NULL` nếu entry nằm ở thư mục gốc.
- `$3`: Tên entry.
- `$4`: Loại entry (`FILE` hoặc `DIRECTORY`).
- Chỉ được xem là trùng khi cả `group_id`, `parent_id`, `name` và `type` đều trùng.
- Unique index `idx_entries_unique_name_in_parent` cũng áp dụng cùng quy tắc này. Truy vấn kiểm tra trước không thay thế được unique index vì vẫn có thể xảy ra race condition giữa bước kiểm tra và insert.

#### 4. Tạo entry mới (upload file hoặc tạo directory)

Một truy vấn xử lý cả hai trường hợp: tạo ở thư mục gốc khi `$3` là SQL `NULL`, hoặc tạo bên trong một directory khi `$3` là ID directory cha.

```sql
INSERT INTO entries (name, type, path, uploader_id, group_id, parent_id)
SELECT $1, $2,
    CASE
        WHEN $3 IS NULL THEN g.dir_path || '/' || $1
        ELSE parent.path || '/' || $1
    END,
    $4, g.id, $3
FROM groups g
LEFT JOIN entries parent
    ON parent.id = $3
    AND parent.group_id = g.id
    AND parent.type = 'DIRECTORY'
WHERE g.id = $5
    AND ($3 IS NULL OR parent.id IS NOT NULL)
RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;
```

- `$1`: Tên entry.
- `$2`: Loại entry (`FILE` hoặc `DIRECTORY`).
- `$3`: ID directory cha; truyền SQL `NULL` nếu tạo ở thư mục gốc.
- `$4`: ID người upload/tạo entry.
- `$5`: ID nhóm.
- Khi `$3` là `NULL`, `path` được tạo từ `groups.dir_path`.
- Khi `$3` có giá trị, parent phải tồn tại, thuộc cùng nhóm và có `type = 'DIRECTORY'`; `path` được tạo từ `parent.path`.
- `RETURNING` trả về tối đa một dòng: một dòng nếu insert thành công, zero rows nếu group hoặc parent không thỏa điều kiện. Lỗi constraint sẽ làm `db_execute()` báo thực thi thất bại.

#### 5. Đổi tên entry

Chỉ trưởng nhóm được phép rename. Hai truy vấn dưới đây phải chạy trong cùng một transaction.

**Bước 1: Cập nhật tên và path của entry được đổi tên.**

```sql
UPDATE entries
SET name = $1, path = $2
WHERE id = $3 AND group_id = $4
RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;
```

- `$1`: Tên mới.
- `$2`: Path mới của entry.
- `$3`: ID entry.
- `$4`: ID nhóm.
- Trả về tối đa một dòng. Zero rows nghĩa là không tìm thấy entry khớp điều kiện.

**Bước 2: Nếu entry là `DIRECTORY`, cập nhật path của toàn bộ descendants.**

```sql
UPDATE entries
SET path = $1 || SUBSTRING(path FROM CHAR_LENGTH($2) + 1)
WHERE group_id = $3
    AND LEFT(path, CHAR_LENGTH($2) + 1) = ($2 || '/');
```

- `$1`: Path mới của directory.
- `$2`: Path cũ của directory.
- `$3`: ID nhóm.
- Chỉ chạy bước này khi entry được rename là `DIRECTORY`.
- Điều kiện so khớp thêm dấu `/` để tránh cập nhật nhầm các path chỉ có cùng tiền tố tên, ví dụ `folder` và `folder-old`.
- Cần lấy và giữ path cũ trước khi cập nhật entry; nếu một trong hai bước thất bại, rollback transaction.

#### 6. Xóa entry

Chỉ trưởng nhóm được phép delete.

```sql
DELETE FROM entries
WHERE id = $1 AND group_id = $2
RETURNING id, name, type, path, uploader_id, group_id, parent_id, uploaded_at;
```

- `$1`: ID entry.
- `$2`: ID nhóm.
- Trả về tối đa một dòng. Zero rows nghĩa là không tìm thấy entry khớp điều kiện.
- Nếu entry bị xóa là directory, các descendants được xóa tự động nhờ `ON DELETE CASCADE` trên khóa ngoại `parent_id`.

---

## 3. Quy ước chung

### 3.1. Đường dẫn file

- Sử dụng thư mục cha chung `root/` cho toàn server.
- Mỗi nhóm có thư mục riêng ngay dưới `root/`, định danh bằng ID nhóm, ví dụ `root/1`, `root/2`, ... để tránh xung đột khi tên nhóm trùng nhau.
- Không nhận đường dẫn vật lý trực tiếp từ client khi upload, download, delete hoặc rename. Client gửi `entry_id` và `group_id`; server kiểm tra quyền và tra cứu `entries.path` trong CSDL.

### 3.2. Quy ước về `parent_id`

- Trong PostgreSQL, entry nằm trực tiếp trong thư mục gốc có `parent_id IS NULL`.
- Khi gọi `PQexecParams()`, truyền con trỏ `NULL` cho tham số `parent_id` để gửi SQL `NULL`; không truyền chuỗi `"NULL"`.
- Khi đọc kết quả, kiểm tra `PQgetisnull(result, row, column)` trước khi gọi `PQgetvalue()`.
- Nếu struct C biểu diễn `parent_id` bằng số nguyên, cần có quy ước hoặc cờ riêng để phân biệt SQL `NULL` với một ID hợp lệ. Không nên mặc định rằng giá trị `0` trong C tự động là SQL `NULL`.
