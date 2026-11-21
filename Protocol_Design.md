# THIẾT KẾ PROTOCOL CHO ỨNG DỤNG CHIA SẺ FILE

## 1. Tổng quan

Giao thức sử dụng là Custom FTP Protocol, được thiết kế dựa trên mô hình giao tiếp của FTP.

- Client và Server giao tiếp với nhau thông qua TCP Socket.
- Server sử dụng một linked list các Session để quản lý các Client đang kết nối.
- Mỗi kết nối tương ứng với một Session trong linked list.
- Mỗi Session lưu trữ socket descriptor (client_fd), ID người dùng (user_id), trạng thái xác thực (authenticated), nhóm hiện tại (current_group_id) và đường dẫn thư mục hiện tại (current_directory).
- Khi Client ngắt kết nối, Server đóng socket, xóa Session tương ứng khỏi linked list và giải phóng bộ nhớ.

### 1.1. Cấu trúc Session


```c
typedef struct Session {
    int client_fd;
    int user_id;
    bool authenticated;
    int current_group_id;
    char current_directory[1024];

    struct Session *next;
} Session;
```

```c
typedef struct Session {
    int control_fd;
    int data_fd;

    int user_id;
    int authenticated;

    int current_group_id;
    char current_directory[PATH_MAX];

    struct Session *next;
} Session;
```

### 1.2. Quy ước giao tiếp

**Request:**

```text
COMMAND argument1 argument2 ...\r\n
```

**Response:**

```text
CODE MESSAGE [DATA]\r\n
```

- `CODE`: Mã response gồm 3 chữ số.
- `MESSAGE`: Thông báo kết quả.
- `DATA`: Dữ liệu bổ sung nếu cần thiết.

Đối với dữ liệu dạng danh sách, Server trả về response header trước, sau đó truyền nội dung qua Data Connection.

Đối với dữ liệu file, không sử dụng chuỗi kết thúc `\r\n` để xác định kích thước dữ liệu. Server và Client phải dựa vào kích thước file đã thống nhất.

## 2. Định nghĩa mã response

### 2.1. Ý nghĩa các nhóm mã

| Nhóm mã | Ý nghĩa |
| :---: | --- |
| `1xx` | Positive Preliminary: Request đã được tiếp nhận, quá trình xử lý đang được chuẩn bị hoặc bắt đầu |
| `2xx` | Positive Completion: Thao tác đã hoàn thành thành công |
| `3xx` | Positive Intermediate: Cần cung cấp thêm thông tin để tiếp tục thao tác |
| `4xx` | Transient Negative: Thao tác thất bại tạm thời, có thể thử lại sau |
| `5xx` | Permanent Negative: Thao tác không thể thành công nếu không thay đổi request hoặc điều kiện thực hiện |

Toàn bộ các lệnh giao thức sử dụng `500 Server Error` làm mã lỗi fallback cuối cùng khi gặp lỗi nội bộ chưa được định nghĩa.

### 2.2. Response code chung

| Code | Message | Ý nghĩa |
| :---: | --- | --- |
| `200` | `Connection Successful` | Kết nối đến Server thành công |
| `201` | `User Disconnected` | Người dùng đã ngắt kết nối |
| `500` | `Server Error` | Có lỗi Server chưa được định nghĩa |

### 2.3. Response code cho xác thực và quản lý tài khoản

| Code | Message | Ý nghĩa |
| :---: | --- | --- |
| `210` | `Register Successful` | Đăng ký tài khoản thành công |
| `211` | `Login Successful` | Đăng nhập thành công |
| `212` | `Logout Successful` | Đăng xuất thành công |
| `510` | `User Existed` | Username đã được sử dụng |
| `511` | `Authentication Failed` | Username hoặc password không chính xác |
| `512` | `Username Missing` | Chưa cung cấp username |
| `513` | `Password Missing` | Chưa cung cấp password |
| `514` | `Already Authenticated` | Người dùng đã đăng nhập |

### 2.4. Response code cho quản lý nhóm

| Code | Message | Ý nghĩa |
| :---: | --- | --- |
| `220` | `Creation Successful` | Tạo nhóm thành công |
| `221` | `Request Sent` | Gửi yêu cầu tham gia nhóm thành công |
| `222` | `Invite Sent` | Gửi lời mời tham gia nhóm thành công |
| `223` | `Request Processed` | Xử lý yêu cầu tham gia thành công |
| `224` | `Invite Processed` | Xử lý lời mời thành công |
| `225` | `Group Listed` | Liệt kê danh sách nhóm thành công |
| `226` | `Group Information Retrieved` | Lấy thông tin nhóm thành công |
| `227` | `Group Left` | Rời nhóm thành công |
| `228` | `Member Removed` | Xóa thành viên khỏi nhóm thành công |
| `510` | `Group Name Missing` | Chưa cung cấp tên nhóm |
| `511` | `Group Not Found` | Không tìm thấy nhóm |
| `512` | `Group Already Joined` | Người dùng đã là thành viên của nhóm |
| `513` | `Group Request Existed` | Yêu cầu tham gia nhóm đã tồn tại |
| `514` | `Group Invite Existed` | Lời mời tham gia nhóm đã tồn tại |
| `515` | `Group Access Denied` | Không có quyền truy cập nhóm |
| `516` | `Not Group Owner` | Người dùng không phải trưởng nhóm |
| `517` | `Group Request Not Found` | Không tìm thấy yêu cầu tham gia |
| `518` | `Group Invite Not Found` | Không tìm thấy lời mời |
| `519` | `Group Member Not Found` | Không tìm thấy thành viên |
| `520` | `Cannot Leave Owned Group` | Trưởng nhóm không thể rời nhóm |
| `521` | `Cannot Remove Owner` | Không thể xóa trưởng nhóm |
| `522` | `Cannot Invite Self` | Không thể tự mời chính mình |

### 2.5. Response code cho quản lý thành viên

| Code | Message | Ý nghĩa |
| :---: | --- | --- |
| `230` | `Member Listed` | Liệt kê thành viên thành công |
| `231` | `Request Listed` | Liệt kê yêu cầu tham gia thành công |
| `232` | `Invite Listed` | Liệt kê lời mời thành công |
| `523` | `Member Already Exists` | Thành viên đã tồn tại |
| `524` | `Cannot Remove Self` | Không thể sử dụng chức năng xóa thành viên để tự rời nhóm |

### 2.6. Response code cho quản lý file và thư mục

| Code | Message | Ý nghĩa |
| :---: | --- | --- |
| `240` | `Directory Created` | Tạo thư mục thành công |
| `241` | `Directory Renamed` | Đổi tên thư mục thành công |
| `242` | `Directory Deleted` | Xóa thư mục thành công |
| `243` | `Directory Copied` | Sao chép thư mục thành công |
| `244` | `Directory Moved` | Di chuyển thư mục thành công |
| `245` | `File Renamed` | Đổi tên file thành công |
| `246` | `File Deleted` | Xóa file thành công |
| `247` | `File Copied` | Sao chép file thành công |
| `248` | `File Moved` | Di chuyển file thành công |
| `249` | `Directory Listed` | Liệt kê nội dung thư mục thành công |
| `250` | `Directory Changed` | Thay đổi thư mục hiện tại thành công |
| `251` | `Current Directory Retrieved` | Lấy thư mục hiện tại thành công |
| `252` | `File Upload Successful` | Upload file thành công |
| `253` | `File Download Successful` | Download file thành công |
| `525` | `File Not Found` | Không tìm thấy file |
| `526` | `Directory Not Found` | Không tìm thấy thư mục |
| `527` | `File Already Exists` | File đã tồn tại tại vị trí đích |
| `528` | `Directory Already Exists` | Thư mục đã tồn tại tại vị trí đích |
| `529` | `Invalid File Name` | Tên file không hợp lệ |
| `530` | `Invalid Directory Name` | Tên thư mục không hợp lệ |
| `531` | `Directory Not Empty` | Thư mục vẫn còn nội dung |
| `532` | `Invalid Destination` | Đường dẫn đích không hợp lệ |
| `533` | `File Access Denied` | Không có quyền truy cập file |
| `534` | `Directory Access Denied` | Không có quyền truy cập thư mục |
| `535` | `Not Group Owner` | Người dùng không phải trưởng nhóm |
| `536` | `File Size Invalid` | Kích thước file không hợp lệ |
| `537` | `Invalid File Type` | Loại đối tượng không hợp lệ |

### 2.7. Response code cho truyền file

| Code | Message | Ý nghĩa |
| :---: | --- | --- |
| `260` | `Transfer Ready` | Server đã sẵn sàng truyền file |
| `261` | `Transfer Started` | Bắt đầu truyền file |
| `262` | `Transfer Complete` | Hoàn tất truyền file |
| `263` | `Transfer Cancelled` | Hủy truyền file thành công |
| `400` | `Connection Interrupted` | Kết nối bị gián đoạn |
| `401` | `Connection Timeout` | Kết nối bị timeout |
| `402` | `Transfer Interrupted` | Quá trình truyền file bị gián đoạn |
| `403` | `Server Temporarily Unavailable` | Server tạm thời không thể xử lý |
| `404` | `Storage Temporarily Unavailable` | Bộ nhớ lưu trữ tạm thời không khả dụng |
| `405` | `Connection Limit Reached` | Đã đạt giới hạn kết nối đồng thời |
| `406` | `Transfer Timeout` | Quá thời gian truyền file |
| `540` | `Insufficient Storage` | Không đủ dung lượng lưu trữ |
| `541` | `Transfer Size Mismatch` | Kích thước dữ liệu không khớp |
| `542` | `Transfer Permission Denied` | Không có quyền truyền file |

### 2.8. Response code cho quản lý phiên

| Code | Message | Ý nghĩa |
| :---: | --- | --- |
| `450` | `Session Expired` | Phiên làm việc đã hết hạn |
| `451` | `Session Not Found` | Không tìm thấy phiên làm việc |
| `452` | `Session Invalid` | Phiên làm việc không hợp lệ |

## 3. Định nghĩa các lệnh giao thức

### 3.1. Đăng ký tài khoản

**Request:**

```text
REGISTER <username> <password>
```

**Quy trình xử lý:**

- B1: Kiểm tra username và password có được cung cấp đầy đủ không.
- B2: Kiểm tra username có trùng với giá trị `name` trong bảng `users` không.
- B3: Nếu chưa tồn tại, tạo tài khoản mới và lưu thông tin vào database.
- B4: Thông báo đăng ký thành công và yêu cầu người dùng đăng nhập.

**Trả về:**

- `210 Register Successful`: Đăng ký tài khoản thành công.
- `510 User Existed`: Username đã được sử dụng.
- `512 Username Missing`: Chưa cung cấp username.
- `513 Password Missing`: Chưa cung cấp password.
- `500 Server Error`: Lỗi Server.

### 3.2. Đăng nhập

**Request:**

```text
LOGIN <username> <password>
```

**Quy trình xử lý:**

- B1: Kiểm tra Session hiện tại đã đăng nhập chưa.
- B2: Kiểm tra username có tồn tại trong bảng `users` không.
- B3: Xác thực password.
- B4: Nếu thông tin chính xác, cập nhật `user_id` và `authenticated` trong Session.
- B5: Duy trì Session cho đến khi người dùng đăng xuất hoặc ngắt kết nối.

**Trả về:**

- `211 Login Successful`: Đăng nhập thành công.
- `511 Authentication Failed`: Username hoặc password không chính xác.
- `514 Already Authenticated`: Người dùng đã đăng nhập.
- `500 Server Error`: Lỗi Server.

### 3.3. Đăng xuất

**Request:**

```text
LOGOUT
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Xóa thông tin người dùng khỏi Session.
- B3: Đặt `user_id = 0` và `authenticated = 0`.
- B4: Đặt lại thư mục và nhóm hiện tại.

**Trả về:**

- `212 Logout Successful`: Đăng xuất thành công.
- `452 Session Invalid`: Session không hợp lệ.
- `500 Server Error`: Lỗi Server.

### 3.4. Tạo nhóm chia sẻ

**Request:**

```text
GROUP_CREATE <group_name>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra tên nhóm có được cung cấp không.
- B3: Tạo nhóm mới trong database.
- B4: Gán người tạo nhóm làm trưởng nhóm.
- B5: Thêm người tạo vào danh sách thành viên của nhóm.
- B6: Tạo thư mục gốc dành riêng cho nhóm.

**Trả về:**

- `220 Creation Successful`: Tạo nhóm thành công.
- `510 Group Name Missing`: Chưa cung cấp tên nhóm.
- `500 Server Error`: Lỗi Server.

### 3.5. Liệt kê danh sách nhóm

**Request:**

```text
GROUP_LIST
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Truy vấn danh sách nhóm mà người dùng đang là thành viên.
- B3: Trả về danh sách nhóm, bao gồm ID, tên nhóm và vai trò của người dùng.

**Trả về:**

- `225 Group Listed`: Liệt kê thành công.
- `500 Server Error`: Lỗi Server.

### 3.6. Lấy thông tin nhóm

**Request:**

```text
GROUP_INFO <group_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra nhóm có tồn tại không.
- B3: Kiểm tra người dùng có phải thành viên của nhóm không.
- B4: Truy vấn thông tin nhóm và trả về cho Client.

**Trả về:**

- `226 Group Information Retrieved`: Lấy thông tin thành công.
- `511 Group Not Found`: Không tìm thấy nhóm.
- `515 Group Access Denied`: Không có quyền truy cập.
- `500 Server Error`: Lỗi Server.

### 3.7. Liệt kê thành viên trong nhóm

**Request:**

```text
MEMBER_LIST <group_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra nhóm có tồn tại không.
- B3: Kiểm tra người dùng có phải thành viên của nhóm không.
- B4: Truy vấn danh sách thành viên của nhóm.
- B5: Trả về danh sách ID, username và vai trò của từng thành viên.

**Trả về:**

- `230 Member Listed`: Liệt kê thành viên thành công.
- `511 Group Not Found`: Không tìm thấy nhóm.
- `515 Group Access Denied`: Không có quyền truy cập.
- `500 Server Error`: Lỗi Server.

### 3.8. Gửi yêu cầu tham gia nhóm

**Request:**

```text
GROUP_JOIN <group_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra nhóm có tồn tại không.
- B3: Kiểm tra người dùng đã là thành viên của nhóm chưa.
- B4: Kiểm tra yêu cầu tham gia trước đó đã tồn tại chưa.
- B5: Tạo yêu cầu tham gia với trạng thái `PENDING`.
- B6: Thông báo gửi yêu cầu thành công.

**Trả về:**

- `221 Request Sent`: Gửi yêu cầu thành công.
- `511 Group Not Found`: Không tìm thấy nhóm.
- `512 Group Already Joined`: Đã là thành viên.
- `513 Group Request Existed`: Yêu cầu đã tồn tại.
- `500 Server Error`: Lỗi Server.

### 3.9. Liệt kê yêu cầu tham gia nhóm

**Request:**

```text
REQUEST_LIST <group_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra nhóm có tồn tại không.
- B3: Kiểm tra người dùng có phải trưởng nhóm không.
- B4: Truy vấn các yêu cầu tham gia có trạng thái `PENDING`.
- B5: Trả về danh sách yêu cầu.

**Trả về:**

- `231 Request Listed`: Liệt kê yêu cầu thành công.
- `511 Group Not Found`: Không tìm thấy nhóm.
- `516 Not Group Owner`: Không phải trưởng nhóm.
- `500 Server Error`: Lỗi Server.

### 3.10. Phê duyệt hoặc từ chối yêu cầu tham gia

**Request:**

```text
REQUEST_PROCESS <group_id> <user_id> <action>
```

Trong đó:

- `action = ACCEPT`: Chấp nhận yêu cầu.
- `action = REJECT`: Từ chối yêu cầu.

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra nhóm có tồn tại không.
- B3: Kiểm tra người dùng hiện tại có phải trưởng nhóm không.
- B4: Kiểm tra yêu cầu tham gia có tồn tại và đang ở trạng thái `PENDING` không.
- B5: Nếu `ACCEPT`, thêm người dùng được yêu cầu vào nhóm.
- B6: Nếu `REJECT`, từ chối yêu cầu và cập nhật trạng thái.
- B7: Cập nhật trạng thái yêu cầu thành `ACCEPTED` hoặc `REJECTED`.

**Trả về:**

- `223 Request Processed`: Xử lý yêu cầu thành công.
- `511 Group Not Found`: Không tìm thấy nhóm.
- `516 Not Group Owner`: Không phải trưởng nhóm.
- `517 Group Request Not Found`: Không tìm thấy yêu cầu.
- `500 Server Error`: Lỗi Server.

### 3.11. Mời người dùng tham gia nhóm

**Request:**

```text
INVITE <group_id> <user_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra nhóm có tồn tại không.
- B3: Kiểm tra người dùng hiện tại có phải trưởng nhóm không.
- B4: Kiểm tra người được mời có tồn tại không.
- B5: Kiểm tra người được mời đã là thành viên chưa.
- B6: Kiểm tra lời mời trước đó đã tồn tại chưa.
- B7: Tạo lời mời với trạng thái `PENDING`.

**Trả về:**

- `222 Invite Sent`: Gửi lời mời thành công.
- `511 Group Not Found`: Không tìm thấy nhóm.
- `516 Not Group Owner`: Không phải trưởng nhóm.
- `512 Group Already Joined`: Người được mời đã là thành viên.
- `514 Group Invite Existed`: Lời mời đã tồn tại.
- `522 Cannot Invite Self`: Không thể tự mời chính mình.
- `500 Server Error`: Lỗi Server.

### 3.12. Liệt kê lời mời tham gia nhóm

**Request:**

```text
INVITE_LIST
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Truy vấn các lời mời gửi đến người dùng hiện tại có trạng thái `PENDING`.
- B3: Trả về danh sách lời mời, bao gồm ID nhóm và thông tin người mời.

**Trả về:**

- `232 Invite Listed`: Liệt kê lời mời thành công.
- `500 Server Error`: Lỗi Server.

### 3.13. Chấp nhận hoặc từ chối lời mời

**Request:**

```text
INVITE_PROCESS <group_id> <action>
```

Trong đó:

- `action = ACCEPT`: Chấp nhận lời mời.
- `action = REJECT`: Từ chối lời mời.

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra lời mời có tồn tại và thuộc về người dùng hiện tại không.
- B3: Kiểm tra lời mời đang ở trạng thái `PENDING`.
- B4: Nếu `ACCEPT`, thêm người dùng vào nhóm.
- B5: Nếu `REJECT`, từ chối lời mời.
- B6: Cập nhật trạng thái lời mời thành `ACCEPTED` hoặc `REJECTED`.

**Trả về:**

- `224 Invite Processed`: Xử lý lời mời thành công.
- `511 Group Not Found`: Không tìm thấy nhóm.
- `518 Group Invite Not Found`: Không tìm thấy lời mời.
- `500 Server Error`: Lỗi Server.

### 3.14. Rời nhóm

**Request:**

```text
GROUP_LEAVE <group_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra nhóm có tồn tại không.
- B3: Kiểm tra người dùng có phải thành viên của nhóm không.
- B4: Kiểm tra người dùng có phải trưởng nhóm không.
- B5: Nếu không phải trưởng nhóm, xóa quan hệ thành viên khỏi nhóm.

**Trả về:**

- `227 Group Left`: Rời nhóm thành công.
- `511 Group Not Found`: Không tìm thấy nhóm.
- `520 Cannot Leave Owned Group`: Trưởng nhóm không thể rời nhóm.
- `515 Group Access Denied`: Không phải thành viên.
- `500 Server Error`: Lỗi Server.

### 3.15. Xóa thành viên khỏi nhóm

**Request:**

```text
GROUP_REMOVE <group_id> <user_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra nhóm có tồn tại không.
- B3: Kiểm tra người dùng hiện tại có phải trưởng nhóm không.
- B4: Kiểm tra thành viên cần xóa có tồn tại trong nhóm không.
- B5: Kiểm tra thành viên cần xóa có phải trưởng nhóm không.
- B6: Xóa quan hệ thành viên khỏi nhóm.

**Trả về:**

- `228 Member Removed`: Xóa thành viên thành công.
- `511 Group Not Found`: Không tìm thấy nhóm.
- `516 Not Group Owner`: Không phải trưởng nhóm.
- `519 Group Member Not Found`: Không tìm thấy thành viên.
- `521 Cannot Remove Owner`: Không thể xóa trưởng nhóm.
- `500 Server Error`: Lỗi Server.

## 4. Quản lý thư mục

### 4.1. Lấy thư mục hiện tại

**Request:**

```text
PWD
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Lấy đường dẫn thư mục hiện tại trong Session.
- B3: Trả về đường dẫn.

**Trả về:**

- `251 Current Directory Retrieved`: Lấy đường dẫn thành công.
- `500 Server Error`: Lỗi Server.

### 4.2. Thay đổi thư mục hiện tại

**Request:**

```text
CWD <directory_path>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra đường dẫn thư mục có tồn tại không.
- B3: Kiểm tra người dùng có quyền truy cập thư mục không.
- B4: Cập nhật thư mục hiện tại trong Session.

**Trả về:**

- `250 Directory Changed`: Thay đổi thư mục thành công.
- `526 Directory Not Found`: Không tìm thấy thư mục.
- `534 Directory Access Denied`: Không có quyền truy cập.
- `500 Server Error`: Lỗi Server.

### 4.3. Liệt kê nội dung thư mục

**Request:**

```text
LIST
```

Hoặc:

```text
LIST <directory_path>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Xác định thư mục cần liệt kê. Nếu không cung cấp đường dẫn, sử dụng thư mục hiện tại.
- B3: Kiểm tra thư mục có tồn tại không.
- B4: Kiểm tra người dùng có quyền truy cập không.
- B5: Lấy danh sách file và thư mục con.
- B6: Trả về thông tin danh sách qua Data Connection.

**Trả về:**

- `249 Directory Listed`: Liệt kê thành công.
- `526 Directory Not Found`: Không tìm thấy thư mục.
- `534 Directory Access Denied`: Không có quyền truy cập.
- `500 Server Error`: Lỗi Server.

### 4.4. Tạo thư mục con

**Request:**

```text
MKDIR <directory_name>
```

Hoặc:

```text
MKDIR <parent_path> <directory_name>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra người dùng có phải thành viên của nhóm chứa thư mục không.
- B3: Kiểm tra tên thư mục hợp lệ.
- B4: Kiểm tra thư mục cha có tồn tại không.
- B5: Kiểm tra thư mục cùng tên đã tồn tại chưa.
- B6: Tạo thư mục con.

**Trả về:**

- `240 Directory Created`: Tạo thư mục thành công.
- `526 Directory Not Found`: Không tìm thấy thư mục cha.
- `528 Directory Already Exists`: Thư mục đã tồn tại.
- `530 Invalid Directory Name`: Tên thư mục không hợp lệ.
- `534 Directory Access Denied`: Không có quyền truy cập.
- `500 Server Error`: Lỗi Server.

### 4.5. Đổi tên thư mục

**Request:**

```text
RENAME_DIR <directory_path> <new_name>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra thư mục cần đổi tên có tồn tại không.
- B3: Kiểm tra người dùng có phải trưởng nhóm chứa thư mục không.
- B4: Kiểm tra tên mới hợp lệ và chưa được sử dụng.
- B5: Đổi tên thư mục.

**Trả về:**

- `241 Directory Renamed`: Đổi tên thành công.
- `526 Directory Not Found`: Không tìm thấy thư mục.
- `516 Not Group Owner`: Không phải trưởng nhóm.
- `530 Invalid Directory Name`: Tên thư mục không hợp lệ.
- `528 Directory Already Exists`: Tên thư mục mới đã tồn tại.
- `500 Server Error`: Lỗi Server.

### 4.6. Xóa thư mục

**Request:**

```text
RMDIR <directory_path>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra thư mục có tồn tại không.
- B3: Kiểm tra người dùng có phải trưởng nhóm không.
- B4: Kiểm tra thư mục có chứa nội dung không.
- B5: Nếu thư mục rỗng, xóa thư mục.

**Trả về:**

- `242 Directory Deleted`: Xóa thư mục thành công.
- `526 Directory Not Found`: Không tìm thấy thư mục.
- `516 Not Group Owner`: Không phải trưởng nhóm.
- `531 Directory Not Empty`: Thư mục không rỗng.
- `500 Server Error`: Lỗi Server.

## 5. Quản lý file

### 5.1. Upload file

**Request:**

```text
UPLOAD <filename> <file_size>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra người dùng có phải thành viên của nhóm chứa thư mục đích không.
- B3: Kiểm tra tên file và kích thước file hợp lệ.
- B4: Kiểm tra file cùng tên đã tồn tại trong thư mục đích chưa.
- B5: Chuẩn bị Data Connection để nhận dữ liệu.
- B6: Thông báo Server đã sẵn sàng nhận file.
- B7: Client truyền nội dung file qua Data Connection.
- B8: Server ghi dữ liệu trực tiếp xuống file trên ổ đĩa, không nạp toàn bộ file vào RAM.
- B9: Sau khi nhận đủ dữ liệu, đóng Data Connection và cập nhật metadata file trong database.
- B10: Ghi log hoạt động và trả về kết quả.

**Trả về:**

- `260 Transfer Ready`: Sẵn sàng nhận file.
- `261 Transfer Started`: Bắt đầu upload.
- `252 File Upload Successful`: Upload thành công.
- `511 Group Not Found`: Không tìm thấy nhóm.
- `515 Group Access Denied`: Không có quyền truy cập nhóm.
- `527 File Already Exists`: File đã tồn tại.
- `536 File Size Invalid`: Kích thước file không hợp lệ.
- `404 Storage Temporarily Unavailable`: Storage tạm thời không khả dụng.
- `402 Transfer Interrupted`: Quá trình upload bị gián đoạn.
- `540 Insufficient Storage`: Không đủ dung lượng lưu trữ.
- `541 Transfer Size Mismatch`: Kích thước dữ liệu không khớp.
- `500 Server Error`: Lỗi Server.

### 5.2. Download file

**Request:**

```text
DOWNLOAD <file_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra file có tồn tại không.
- B3: Kiểm tra người dùng có phải thành viên của nhóm chứa file không.
- B4: Xác định kích thước file.
- B5: Chuẩn bị Data Connection.
- B6: Thông báo Server đã sẵn sàng gửi file.
- B7: Server truyền nội dung file qua Data Connection.
- B8: Sau khi truyền hoàn tất, đóng Data Connection và ghi log hoạt động.

**Trả về:**

- `260 Transfer Ready`: Sẵn sàng gửi file.
- `261 Transfer Started`: Bắt đầu download.
- `253 File Download Successful`: Download thành công.
- `525 File Not Found`: Không tìm thấy file.
- `515 Group Access Denied`: Không có quyền truy cập nhóm.
- `402 Transfer Interrupted`: Quá trình download bị gián đoạn.
- `500 Server Error`: Lỗi Server.

### 5.3. Đổi tên file

**Request:**

```text
RENAME <file_id> <new_name>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra file có tồn tại không.
- B3: Kiểm tra người dùng có phải trưởng nhóm chứa file không.
- B4: Kiểm tra tên file mới hợp lệ.
- B5: Kiểm tra tên file mới chưa được sử dụng tại vị trí đích.
- B6: Đổi tên file và cập nhật metadata.

**Trả về:**

- `245 File Renamed`: Đổi tên thành công.
- `525 File Not Found`: Không tìm thấy file.
- `535 Not Group Owner`: Không phải trưởng nhóm.
- `529 Invalid File Name`: Tên file không hợp lệ.
- `527 File Already Exists`: Tên file mới đã tồn tại.
- `500 Server Error`: Lỗi Server.

### 5.4. Xóa file

**Request:**

```text
DELETE <file_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra file có tồn tại không.
- B3: Kiểm tra người dùng có phải trưởng nhóm chứa file không.
- B4: Xóa file khỏi hệ thống lưu trữ.
- B5: Xóa hoặc cập nhật metadata file trong database.
- B6: Ghi log hoạt động.

**Trả về:**

- `246 File Deleted`: Xóa file thành công.
- `525 File Not Found`: Không tìm thấy file.
- `535 Not Group Owner`: Không phải trưởng nhóm.
- `500 Server Error`: Lỗi Server.

### 5.5. Sao chép file

**Request:**

```text
COPY <file_id> <destination_directory_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra file nguồn có tồn tại không.
- B3: Kiểm tra thư mục đích có tồn tại không.
- B4: Kiểm tra người dùng có quyền truy cập cả nguồn và đích không.
- B5: Kiểm tra quyền thực hiện thao tác sao chép theo chính sách nhóm.
- B6: Kiểm tra tên file tại thư mục đích có bị trùng không.
- B7: Sao chép nội dung file và tạo metadata mới.
- B8: Ghi log hoạt động.

**Trả về:**

- `247 File Copied`: Sao chép thành công.
- `525 File Not Found`: Không tìm thấy file nguồn.
- `526 Directory Not Found`: Không tìm thấy thư mục đích.
- `527 File Already Exists`: File đã tồn tại tại vị trí đích.
- `535 Not Group Owner`: Không đủ quyền sao chép.
- `500 Server Error`: Lỗi Server.

### 5.6. Di chuyển file

**Request:**

```text
MOVE <file_id> <destination_directory_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra file nguồn có tồn tại không.
- B3: Kiểm tra thư mục đích có tồn tại không.
- B4: Kiểm tra quyền truy cập nguồn và đích.
- B5: Kiểm tra quyền thực hiện thao tác di chuyển.
- B6: Kiểm tra tên file tại thư mục đích có bị trùng không.
- B7: Di chuyển file và cập nhật metadata.
- B8: Ghi log hoạt động.

**Trả về:**

- `248 File Moved`: Di chuyển thành công.
- `525 File Not Found`: Không tìm thấy file nguồn.
- `526 Directory Not Found`: Không tìm thấy thư mục đích.
- `527 File Already Exists`: File đã tồn tại tại vị trí đích.
- `535 Not Group Owner`: Không đủ quyền di chuyển.
- `500 Server Error`: Lỗi Server.

## 6. Sao chép và di chuyển thư mục

### 6.1. Sao chép thư mục

**Request:**

```text
COPY_DIR <directory_id> <destination_directory_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra thư mục nguồn và thư mục đích có tồn tại không.
- B3: Kiểm tra quyền truy cập hai thư mục.
- B4: Kiểm tra quyền thực hiện sao chép.
- B5: Kiểm tra thư mục đích có nằm bên trong thư mục nguồn không.
- B6: Sao chép toàn bộ cấu trúc thư mục và các file con.
- B7: Tạo metadata mới cho các đối tượng được sao chép.
- B8: Ghi log hoạt động.

**Trả về:**

- `243 Directory Copied`: Sao chép thành công.
- `526 Directory Not Found`: Không tìm thấy thư mục.
- `535 Not Group Owner`: Không đủ quyền sao chép.
- `528 Directory Already Exists`: Thư mục đích đã tồn tại.
- `532 Invalid Destination`: Thư mục đích không hợp lệ.
- `500 Server Error`: Lỗi Server.

### 6.2. Di chuyển thư mục

**Request:**

```text
MOVE_DIR <directory_id> <destination_directory_id>
```

**Quy trình xử lý:**

- B1: Kiểm tra người dùng đã đăng nhập chưa.
- B2: Kiểm tra thư mục nguồn và thư mục đích có tồn tại không.
- B3: Kiểm tra quyền truy cập hai thư mục.
- B4: Kiểm tra quyền thực hiện di chuyển.
- B5: Kiểm tra thư mục đích có nằm bên trong thư mục nguồn không.
- B6: Di chuyển thư mục và cập nhật metadata.
- B7: Ghi log hoạt động.

**Trả về:**

- `244 Directory Moved`: Di chuyển thành công.
- `526 Directory Not Found`: Không tìm thấy thư mục.
- `535 Not Group Owner`: Không đủ quyền di chuyển.
- `528 Directory Already Exists`: Thư mục đích đã tồn tại.
- `532 Invalid Destination`: Thư mục đích không hợp lệ.
- `500 Server Error`: Lỗi Server.

## 7. Ngắt kết nối

### 7.1. Thoát khỏi Server

**Request:**

```text
QUIT
```

**Quy trình xử lý:**

- B1: Kiểm tra Session hiện tại.
- B2: Đóng Data Connection nếu đang tồn tại.
- B3: Đóng Control Connection.
- B4: Xóa Session khỏi linked list.
- B5: Giải phóng bộ nhớ cấp phát cho Session.

**Trả về:**

- `201 User Disconnected`: Ngắt kết nối thành công.

## 8. Quy định về quyền truy cập

Tất cả các thao tác truy cập tài nguyên đều phải được kiểm tra tại Server.

| Chức năng | Trưởng nhóm | Thành viên |
|---|:---:|:---:|
| Xem danh sách file | Có | Có |
| Download file | Có | Có |
| Upload file | Có | Có |
| Tạo thư mục con | Có | Có |
| Đổi tên file | Có | Không |
| Xóa file | Có | Không |
| Copy file | Có | Không |
| Move file | Có | Không |
| Đổi tên thư mục | Có | Không |
| Xóa thư mục | Có | Không |
| Copy thư mục | Có | Không |
| Move thư mục | Có | Không |
| Xem danh sách thành viên | Có | Có |
| Mời thành viên | Có | Không |
| Phê duyệt yêu cầu tham gia | Có | Không |
| Xóa thành viên | Có | Không |

Lưu ý: Quyền copy và move được quy định là quyền của trưởng nhóm trong thiết kế này để tránh thành viên tự ý thay đổi cấu trúc chia sẻ. Đây là quyết định thiết kế bổ sung, vì yêu cầu đề bài chưa chỉ rõ quyền đối với hai thao tác này.

## 9. Quy định về truyền dữ liệu

### 9.1. Control Connection

- Sử dụng TCP để truyền command và response.
- Command kết thúc bằng `\r\n`.
- Server phải xử lý dữ liệu theo luồng, không giả định một lần `recv()` sẽ nhận đầy đủ một command.
- Mỗi Session có một Control Connection riêng.

### 9.2. Data Connection

Sử dụng Passive Mode để thiết lập Data Connection.

**Request:**

```text
PASV
```

**Response:**

```text
227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
```

Client kết nối đến địa chỉ IP và port được Server cung cấp.

Port được xác định bằng:

```text
port = p1 * 256 + p2
```

Data Connection được sử dụng cho:

- Truyền nội dung file upload.
- Truyền nội dung file download.
- Truyền danh sách file và thư mục.

### 9.3. Truyền file có kích thước lớn

- Client gửi tên file và kích thước file trước khi truyền.
- Server xác nhận đã sẵn sàng nhận file.
- Client truyền dữ liệu theo từng buffer.
- Server ghi từng buffer trực tiếp xuống ổ đĩa.
- Không yêu cầu cấp phát một vùng nhớ có kích thước bằng toàn bộ file.
- Sau khi truyền đủ dữ liệu, Server kiểm tra kích thước và hoàn tất thao tác.
- Nếu kết nối bị ngắt, Server đóng file và xử lý dữ liệu upload chưa hoàn tất theo chính sách đã định.

Ví dụ:

```text
Client -> Server:
UPLOAD report.pdf 104857600

Server -> Client:
260 Transfer Ready

Client -> Server:
[Binary Data - 104857600 bytes]

Server -> Client:
252 File Upload Successful
```

## 10. Ghi log hoạt động

Server ghi lại các hoạt động quan trọng của người dùng, bao gồm:

- Đăng ký và đăng nhập.
- Tạo nhóm.
- Gửi yêu cầu tham gia nhóm.
- Chấp nhận hoặc từ chối yêu cầu.
- Mời thành viên.
- Rời nhóm hoặc xóa thành viên.
- Upload và download file.
- Tạo, đổi tên, xóa thư mục.
- Đổi tên, xóa, copy và move file.
- Copy và move thư mục.

Thông tin log tối thiểu:

| Thuộc tính | Ý nghĩa |
|---|---|
| `user_id` | Người thực hiện thao tác |
| `action` | Loại thao tác |
| `target_id` | ID đối tượng bị tác động |
| `group_id` | Nhóm liên quan |
| `created_at` | Thời gian thực hiện |
| `status` | Kết quả thao tác |

Việc ghi log được thực hiện phía Server, không cho phép Client tự gửi log thay cho Server.

---

# 11. Quy ước triển khai

1. Mỗi Control Connection được quản lý bởi một Session riêng biệt trong linked list.
2. Chỉ những Session đã xác thực mới được sử dụng các chức năng quản lý nhóm và file.
3. Mỗi request phải được kiểm tra quyền truy cập trước khi thực hiện thao tác.
4. Mọi thao tác thay đổi dữ liệu phải được ghi log sau khi hoàn thành.
5. Các thao tác file phải kiểm tra đường dẫn để ngăn truy cập ra ngoài thư mục lưu trữ của ứng dụng.
6. File được truyền dưới dạng binary data, không xử lý như chuỗi ký tự.
7. Các thao tác upload, download phải hỗ trợ file có kích thước lớn bằng cách truyền dữ liệu theo từng buffer.
8. Server không được tin tưởng thông tin `user_id` do Client gửi lên. Danh tính người dùng phải được lấy từ Session đã xác thực.
9. Mọi thao tác trên database và filesystem cần xử lý lỗi nhất quán, tránh metadata và file thực tế bị lệch nhau.
10. Khi Client ngắt kết nối bất thường, Server phải giải phóng Session và các tài nguyên liên quan.

