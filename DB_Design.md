# Thiết kế CSDL cho ứng dụng chia sẻ file (PostgreSQL 16.0 hoặc mới hơn)

## 1. Danh sách các bảng

### 1.1. ```users```

Bảng này sẽ chứa thông tin người dùng, phục vụ cho việc đăng nhập, đăng ký.

Với cột ```name```, không được trùng lặp tên user.

```sql
CREATE TABLE users (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(64) UNIQUE NOT NULL,
    password VARCHAR(255) NOT NULL,

    created_at TIMESTAMPZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_login_at TIMESTAMPZ
);
```

### 1.2. ```groups```

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

### 1.3. ```users_groups```

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

### 1.4. ```entries```

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

### 1.5. ```requests```

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

## 2. Danh sách các lệnh truy vấn
### 2.1. Truy vấn phục vụ đăng nhập, đăng ký
#### 1. Kiểm tra username có tồn tại không (cho cả đăng nhập và đăng ký)

```sql
SELECT EXISTS (
    SELECT 1
    FROM users
    WHERE name = $1;
);
```

- ```$1```: Placeholder cho username dùng để truy vấn


#### 2. Kiểm tra password có chính xác không (cho đăng nhập)

**CHÚ Ý**: Ở đây ta so sánh 2 string sau khi mã hoá, không so sánh plain text.

```sql
SELECT EXISTS (
    SELECT 1
    FROM users
    WHERE name = $1
    AND password = $2;
);
```

- ```$1```: Placeholder cho username dùng để truy vấn.
- ```$2```: Placeholder cho password string sau khi mã hoá.

### 2.2. Truy vấn phục vụ quản lý nhóm chia sẻ file
#### 1. Lấy toàn bộ các thành viên trong nhóm (hiển thị chung cho cả nhóm)

```sql
SELECT users.* FROM users
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
)
UPDATE groups
SET dir_path = 'root/' || id
WHERE id = (SELECT id FROM new_group)
RETURNING *;
```

- ```$1```: Placeholder cho biến name của nhóm muốn tạo.
- ```$2```: Placeholder cho biến ID của người tạo nhóm.

#### 5. Tạo entry mới

```sql
INSERT INTO entries (
    name, type, path, uploader_id, group_id, parent_id
)
SELECT
    $1,
    $2,
    parent.path || '/' || $1,
    $3,
    parent.group_id,
    parent.id
FROM entries AS parent
WHERE parent.id = $4
    AND parent.group_id = $5
    AND parent.type = 'DIRECTORY'
RETURNING *;
```

- ```$1```: Placeholder cho biến tên file muốn upload hoặc thư mục con muốn tạo
- ```$2```: Placeholder cho biến loại entry (là ```FILE``` nếu upload file, ```DIRECTORY``` nếu tạo thư mục mới)
- ```$3```: Placeholder cho biến ID của người upload file hoặc tạo thư mục con


## 3. Quy ước chung
### 3.1. Đường dẫn file

Sử dụng một tệp cha chung đặt tên là ```root/``` cho toàn server.

Mỗi nhóm có một tệp con đặt ngay dưới tệp cha chung. Ví dụ: ```root/1```, ```root/2```,.... Những tệp này được gọi là **tệp gốc của nhóm**.

Sử dụng ```id``` của nhóm thay vì sử dụng ```name``` để tránh việc hai nhóm trùng tên sử dụng chung một không gian lưu trữ.

**Không** sử dụng trực tiếp đường dẫn file khi thực hiện request download hoặc upload, sử dụng id trong CSDL và truy vấn cột ```path``` từ đó để lấy nội dung file.

**CHÚ Ý**: TOÀN BỘ các entry nên có đường dẫn xuất phát từ ```root``` để thuận tiện trong việc truy vấn cây file để lấy nội dung file.

### 3.2. Quy ước trong cơ sở dữ liệu

Một tệp nằm ở chính **tệp gốc của nhóm** nếu ```parent_id``` của tệp đó có giá trị là ```NULL```.

Ví dụ: Nếu tệp ```text.txt``` nằm ở ```root/1/text.txt``` thì ```parent_id``` của entry ứng với nó là ```NULL```.