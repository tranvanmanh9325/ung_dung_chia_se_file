#ifndef FILESHARE_DB_PASSWORD_H
#define FILESHARE_DB_PASSWORD_H

#include <stdbool.h>
#include <stddef.h>

#define PASSWORD_HASH_MAX_LEN 128

/**
 * Bam mat khau plain text kem salt ngau nhien theo dinh dang:
 * "$s256$<32_hex_salt>$<64_hex_sha256>"
 *
 * @param plain     Mat khau goc (khong duoc NULL hoac rong)
 * @param out_buf   Bo dem nhan chuoi hash
 * @param out_size  Kich thuoc bo dem (toi thieu PASSWORD_HASH_MAX_LEN)
 * @return 0 neu thanh cong, -1 neu tham so khong hop le
 */
int password_hash(const char *plain, char *out_buf, size_t out_size);

/**
 * Kiem tra mat khau plain text co khop voi chuoi hash da luu trong CSDL khong.
 * Ho tro ca dinh dang "$s256$<salt>$<hash>" va so sanh constant-time.
 *
 * @param plain        Mat khau nguoi dung nhap
 * @param stored_hash  Chuoi hash luu trong bang users.password
 * @return true neu khop, false neu sai hoac loi dinh dang
 */
bool password_verify(const char *plain, const char *stored_hash);

/**
 * Kiem tra username hop le (1..63 ky tu, chi chua chu cai, chu so, '_', '-', '.')
 */
bool db_is_valid_username(const char *username);

/**
 * Kiem tra ten file / thu muc hop le:
 * - Do dai 1..254 ky tu
 * - Khong chua '/' hoac '\\'
 * - Khong phai "." hoac ".."
 */
bool db_is_valid_entry_name(const char *name);

#endif /* FILESHARE_DB_PASSWORD_H */
