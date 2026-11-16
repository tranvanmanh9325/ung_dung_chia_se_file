# ung_dung_chia_se_file

Ứng dụng chia sẻ file cho môn Lập trình mạng.

Trong Ubuntu trên WSL, cài công cụ build và đo coverage:

```sh
sudo apt install build-essential libpq-dev gcovr
```

Tại thư mục repo:

```sh
make all
./bin/server
./bin/client
make test
make coverage
make clean
```

Server và client hiện in một dòng khi chạy. Bước này chỉ cần thư viện
`libpq` để liên kết server, chưa cần dựng PostgreSQL.
