# EC2Study — 概要メモ

ローカル検証用の TLS 対応 MQTT 環境メモ。

## 必要環境

- Docker Desktop 環境。
- PowerShell または Bash 環境。
- OpenSSL（証明書生成用）環境。

## クイックスタート（要点）

1. 証明書生成（初回のみ）。

```bash
mkdir -p docker/mosquitto-certs
openssl genrsa -out docker/mosquitto-certs/ca.key 2048
openssl req -x509 -new -nodes -key docker/mosquitto-certs/ca.key -sha256 -days 3650 -out docker/mosquitto-certs/ca.crt -subj "/CN=MyMosquittoCA"
openssl genrsa -out docker/mosquitto-certs/server.key 2048
openssl req -new -key docker/mosquitto-certs/server.key -out docker/mosquitto-certs/server.csr -subj "/CN=mosquitto"
"subjectAltName=DNS:localhost,DNS:mosquitto,IP:127.0.0.1" | Out-File -FilePath server.ext -Encoding ascii
openssl x509 -req -in docker/mosquitto-certs/server.csr -CA docker/mosquitto-certs/ca.crt -CAkey docker/mosquitto-certs/ca.key -CAcreateserial -out docker/mosquitto-certs/server.crt -days 365 -sha256 -extfile server.ext
Remove-Item server.ext
chmod 644 docker/mosquitto-certs/server.key docker/mosquitto-certs/ca.key
```

2. Docker 起動（サービス起動）。

```bash
cd docker
docker-compose up -d
```

3. Publisher 実行（ビルド済みコンテナ利用）。

```bash
docker run --rm -v "${pwd}:/work" --add-host=host.docker.internal:host-gateway --entrypoint /bin/bash paho-wolfssl-builder -c "/work/docker/build-paho-wolfssl/build_and_package.sh && /work/output/publisher_bin"
```

4. Subscriber 実行（受信監視）。

```bash
cd src
python subscribe/bridge.py
```

## よく使うコマンド

- Broker ログ確認：`docker logs mosquitto_broker --follow`。
- 残存 Lambda コンテナ確認：`docker ps --filter "name=localstack-main-lambda"`。

## 注意点（開発環境向け）

- 証明書は自己署名を利用しているため、本番では正式 CA 発行証明書の利用が必須。
- 開発用に TLS 検証を無効化している箇所あり。本番では `enableServerCertAuth=1` および `tls_insecure_set(False)` に変更必須。
- 現在は本番環境向けに検証有効

詳細は `docs/README.md` を参照。
