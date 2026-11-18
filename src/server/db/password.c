#include "password.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SALT_BYTES   16
#define SALT_HEX_LEN (SALT_BYTES * 2)
#define HASH_HEX_LEN 64
#define HASH_ROUNDS  4096U

static uint64_t rotl64(uint64_t x, unsigned int r)
{
    return (x << r) | (x >> (64U - r));
}

/*
 * Ham bam 256-bit co salt va lap nhieu vong (key stretching) dua tren phep
 * tron ARX (Add-Rotate-Xor) 4x64-bit state.
 */
static void compute_salted_digest(const uint8_t salt[SALT_BYTES],
                                  const char *plain,
                                  char out_hex[HASH_HEX_LEN + 1])
{
    uint64_t s0 = 0x736f6d6570736575ULL;
    uint64_t s1 = 0x646f72616e646f6dULL;
    uint64_t s2 = 0x6c7967656e657261ULL;
    uint64_t s3 = 0x7465646279746573ULL;
    size_t plain_len = strlen(plain);

    for (size_t i = 0; i < SALT_BYTES; ++i) {
        uint64_t v = (uint64_t)salt[i] << ((i % 8U) * 8U);
        if (i < 8) {
            s0 ^= v;
        } else {
            s1 ^= v;
        }
    }

    for (unsigned int r = 0; r < HASH_ROUNDS; ++r) {
        s0 += (uint64_t)r ^ (uint64_t)plain_len;
        for (size_t i = 0; i < plain_len; ++i) {
            uint64_t b = (unsigned char)plain[i];
            s0 = rotl64(s0 + b + (uint64_t)i, 13) ^ s2;
            s1 = rotl64(s1 ^ (b * 0x100000001b3ULL), 17) + s3;
            s2 = rotl64(s2 + s0, 29) ^ s1;
            s3 = rotl64(s3 ^ s1, 31) + s0;
        }
        for (size_t i = 0; i < SALT_BYTES; ++i) {
            uint64_t b = salt[i];
            s2 = rotl64(s2 + b + r, 19) ^ s0;
            s3 = rotl64(s3 ^ (b + i), 23) + s1;
            s0 ^= s3;
            s1 += s2;
        }
    }

    snprintf(out_hex, HASH_HEX_LEN + 1,
             "%016llx%016llx%016llx%016llx",
             (unsigned long long)s0,
             (unsigned long long)s1,
             (unsigned long long)s2,
             (unsigned long long)s3);
}

static void generate_salt(uint8_t salt[SALT_BYTES])
{
    FILE *fp = fopen("/dev/urandom", "rb");
    if (fp != NULL) {
        size_t n = fread(salt, 1, SALT_BYTES, fp);
        fclose(fp);
        if (n == SALT_BYTES) {
            return;
        }
    }

    static unsigned int seeded = 0;
    if (!seeded) {
        struct timespec ts;
        if (timespec_get(&ts, TIME_UTC) != 0) {
            srand((unsigned int)(ts.tv_nsec ^ ts.tv_sec));
        } else {
            srand((unsigned int)time(NULL));
        }
        seeded = 1;
    }

    for (size_t i = 0; i < SALT_BYTES; ++i) {
        salt[i] = (uint8_t)(rand() & 0xFF);
    }
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool constant_time_eq(const char *a, const char *b, size_t len)
{
    unsigned char diff = 0;
    for (size_t i = 0; i < len; ++i) {
        diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    }
    return diff == 0;
}

int password_hash(const char *plain, char *out_buf, size_t out_size)
{
    if (plain == NULL || plain[0] == '\0' || out_buf == NULL ||
        out_size < PASSWORD_HASH_MAX_LEN) {
        return -1;
    }

    uint8_t salt[SALT_BYTES];
    char salt_hex[SALT_HEX_LEN + 1];
    char digest_hex[HASH_HEX_LEN + 1];

    generate_salt(salt);
    for (size_t i = 0; i < SALT_BYTES; ++i) {
        snprintf(&salt_hex[i * 2], 3, "%02x", salt[i]);
    }
    salt_hex[SALT_HEX_LEN] = '\0';

    compute_salted_digest(salt, plain, digest_hex);

    int written = snprintf(out_buf, out_size, "$s256$%s$%s", salt_hex, digest_hex);
    return (written > 0 && (size_t)written < out_size) ? 0 : -1;
}

bool password_verify(const char *plain, const char *stored_hash)
{
    if (plain == NULL || plain[0] == '\0' || stored_hash == NULL) {
        return false;
    }

    const char *prefix = "$s256$";
    size_t prefix_len = strlen(prefix);
    if (strncmp(stored_hash, prefix, prefix_len) != 0) {
        return false;
    }

    const char *salt_ptr = stored_hash + prefix_len;
    if (strlen(salt_ptr) < SALT_HEX_LEN + 1 + HASH_HEX_LEN) {
        return false;
    }
    if (salt_ptr[SALT_HEX_LEN] != '$') {
        return false;
    }

    uint8_t salt[SALT_BYTES];
    for (size_t i = 0; i < SALT_BYTES; ++i) {
        int hi = hex_val(salt_ptr[i * 2]);
        int lo = hex_val(salt_ptr[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        salt[i] = (uint8_t)((hi << 4) | lo);
    }

    const char *expected_digest = salt_ptr + SALT_HEX_LEN + 1;
    if (strlen(expected_digest) != HASH_HEX_LEN) {
        return false;
    }

    char actual_digest[HASH_HEX_LEN + 1];
    compute_salted_digest(salt, plain, actual_digest);

    return constant_time_eq(actual_digest, expected_digest, HASH_HEX_LEN);
}

bool db_is_valid_username(const char *username)
{
    if (username == NULL) {
        return false;
    }
    size_t len = strlen(username);
    if (len == 0 || len >= 64) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)username[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

bool db_is_valid_entry_name(const char *name)
{
    if (name == NULL) {
        return false;
    }
    size_t len = strlen(name);
    if (len == 0 || len >= 255) {
        return false;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (name[i] == '/' || name[i] == '\\') {
            return false;
        }
    }
    return true;
}
