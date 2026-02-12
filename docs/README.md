## ドキュメント概要
ローカル検証用の TLS 対応 MQTT 環境メモ。

## 前提環境
- Docker Desktop のインストール環境。
- PowerShell または Bash の利用環境。
- OpenSSL による証明書生成環境。

## クイックスタート（要点）

1. 証明書生成（初回のみ）：Mosquitto 用の CA とサーバ証明書の生成手順。

```bash
mkdir -p docker/mosquitto-certs
openssl genrsa -out docker/mosquitto-certs/ca.key 2048
openssl req -x509 -new -nodes -key docker/mosquitto-certs/ca.key -sha256 -days 3650 -out docker/mosquitto-certs/ca.crt -subj "/CN=MyMosquittoCA"
openssl genrsa -out docker/mosquitto-certs/server.key 2048
openssl req -new -key docker/mosquitto-certs/server.key -out docker/mosquitto-certs/server.csr -subj "/CN=mosquitto"
openssl x509 -req -in docker/mosquitto-certs/server.csr -CA docker/mosquitto-certs/ca.crt -CAkey docker/mosquitto-certs/ca.key -CAcreateserial -out docker/mosquitto-certs/server.crt -days 365 -sha256
chmod 644 docker/mosquitto-certs/server.key docker/mosquitto-certs/ca.key
```

2. Docker 起動：`docker-compose up -d` によるサービス起動。

```bash
cd docker
docker-compose up -d
```

3. Publisher 実行：Paho ビルド済みコンテナによる Publisher 実行コマンド。

```bash
docker run --rm -v C:\Users\ychxy\OneDrive\Desktop\GIT\EC2study:/work --add-host=host.docker.internal:host-gateway --entrypoint /bin/bash paho-wolfssl-builder -c "/work/docker/build-paho-wolfssl/build_and_package.sh && /work/output/publisher_bin"
```

4. Subscriber 実行：`bridge.py` による受信と Lambda 呼び出し（LocalStack 利用時）。

```bash
cd src
python subscribe/bridge.py
```

## 実装メモ

- Publisher（`publisher.c`）：30 秒間隔の送信ループと SIGINT によるクリーンアップ処理。
- エラーハンドリング：送信失敗時の再接続処理。
- Subscriber（`bridge.py`）：MQTT 受信で Lambda を呼び出す設計、開発用に TLS 検証無効化。
- LocalStack：Lambda 実行時に一時コンテナを作成する挙動に注意。

## コンテナ／ネットワークのクリーンアップ手順

1. 残存コンテナの確認と停止コマンド。

```powershell
docker ps --filter "name=localstack-main-lambda" --format "{{.ID}} {{.Names}}"
docker stop <container-id>
docker rm <container-id>
docker-compose down
```

2. 強制クリーンアップ（必要時）。

```powershell
docker rm -f $(docker ps -aq --filter "name=localstack-main-lambda") || true
docker network prune -f
```

## トラブルシューティング（要点）

- 接続失敗（`return code -1`）：証明書パス、ポート、ファイル権限の確認。
- 送信失敗（`return code -3`）：ブローカー状態の確認（`docker logs mosquitto_broker`）。

## 補足

- 本番環境では正式な CA 発行証明書の利用と TLS 検証の有効化が必要。
- 詳細と背景はコミットログおよび `docs/diagram.mmd` を参照。


# EC2Study - MQTT TLS セキュリティ実装プロジェクト

## 概要

Docker Desktop を用いた疑似的な MQTT 通信環境。C言語 Publisher と Python Subscriber が、TLS 暗号化通信でメッセージのやり取りを実行。セキュリティ強化のためにポート 1883 (平文) から 8883 (TLS) へ移行、自己署名証明書によるセキュアな MQTT 通信を実装。

構成図は [Mermaid](https://mermaid.live) で表示可能。diagram.mmd を Mermaid Live エディタに貼り付け。

## 必要環境

- Docker Desktop
- PowerShell または Bash
- OpenSSL (証明書生成用)

## 実行手順

### 1. 事前準備：TLS 証明書生成

ブローカー用の CA・サーバ証明書を生成。

```bash
mkdir -p docker/mosquitto-certs

# CA 秘密鍵・証明書作成
openssl genrsa -out docker/mosquitto-certs/ca.key 2048
openssl req -x509 -new -nodes -key docker/mosquitto-certs/ca.key -sha256 -days 3650 -out docker/mosquitto-certs/ca.crt -subj "/CN=MyMosquittoCA"

# サーバ秘密鍵・CSR・署名済み証明書作成
openssl genrsa -out docker/mosquitto-certs/server.key 2048
openssl req -new -key docker/mosquitto-certs/server.key -out docker/mosquitto-certs/server.csr -subj "/CN=mosquitto"
"subjectAltName=DNS:localhost,DNS:mosquitto,IP:127.0.0.1" | Out-File -FilePath server.ext -Encoding ascii
openssl x509 -req -in docker/mosquitto-certs/server.csr -CA docker/mosquitto-certs/ca.crt -CAkey docker/mosquitto-certs/ca.key -CAcreateserial -out docker/mosquitto-certs/server.crt -days 365 -sha256 -extfile server.ext
Remove-Item server.ext

# ファイル権限調整
chmod 644 docker/mosquitto-certs/server.key docker/mosquitto-certs/ca.key
```

### 2. Mosquitto ブローカー起動

```bash
cd docker
docker-compose up -d
```

### 3. Publisher (C言語) ビルド・実行

Paho MQTT C ライブラリを OpenSSL TLS サポート付きでビルド、Publisher をコンパイル・実行。

```bash
docker run --rm -v C:\Users\ychxy\OneDrive\Desktop\GIT\EC2study:/work --add-host=host.docker.internal:host-gateway --entrypoint /bin/bash paho-wolfssl-builder -c "/work/docker/build-paho-wolfssl/build_and_package.sh && /work/output/publisher_bin"
```

出力例：
```
Message with delivery token 1 delivered
```

### 4. Subscriber (Python) 実行

別ターミナルで bridge.py を実行。MQTT メッセージ受信を監視。

```bash
cd src
python subscribe/bridge.py
```

受信成功時の出力：
```
【MQTT受信】トピック: sensor/data / データ: {"device_id": "C-Client-01", "temperature": 24.5, "status": "OK"}
```

## セキュリティ実装詳細

### 暗号化通信：TLS 1.2

| コンポーネント | ポート | プロトコル | 説明 |
|---|---|---|---|
| Mosquitto リスナー | 8883 | MQTT over TLS | セキュア通信用 |
| Publisher | 8883 | ssl:// スキーム | OpenSSL ベース TLS 接続 |
| Subscriber | 8883 | Python ssl + paho-mqtt | TLS 検証無効化 (開発用) |

### 証明書構成

- **CA 証明書**：`docker/mosquitto-certs/ca.crt` - ブローカー認証用
- **サーバ証明書**：`docker/mosquitto-certs/server.crt` - Mosquitto TLS 用
- **サーバ秘密鍵**：`docker/mosquitto-certs/server.key` - Mosquitto TLS 用

### ソースコード修正

**Publisher (publisher.c)**
- アドレス：`ssl://host.docker.internal:8883` (TLS 有効)
- SSL オプション：証明書検証無効化 (`enableServerCertAuth = 0`)

**Subscriber (bridge.py)**
- 接続先：`host.docker.internal:8883`
- `client.tls_set()` と `tls_insecure_set(True)` で自己署名証明書対応

**Mosquitto (mosquitto.conf)**
- リスナー：ポート 8883、MQTT プロトコル
- 証明書パス：`/mosquitto/config/certs/` (コンテナ内マウント位置)

**Docker 設定 (docker-compose.yml)**
- ポートマッピング：`8883:8883`
- ボリュームマウント：`./mosquitto-certs:/mosquitto/config/certs:ro`

## Terraform 実行

```bash
cd terraform
terraform init
terraform plan
terraform apply
```

## トラブルシューティング

### 接続失敗 (return code -1)

**原因**：ブローカーのアドレス・ポート、または TLS 設定が不正

**確認方法**：
```bash
# Docker ネットワークから 8883 への疎通確認
docker run --rm --add-host=host.docker.internal:host-gateway alpine:latest /bin/sh -c "apk add netcat-openbsd && nc -zv host.docker.internal 8883"
```

**対処**：
- mosquitto.conf の certfile/keyfile パスが正確か確認
- 証明書ファイル権限が正しいか確認 (`chmod 644`)

### localhost 使用時の接続エラー

**原因**：コンテナ内の localhost はコンテナ自身を指す

**対処**：ホスト接続時は必ず `host.docker.internal` を使用

### Mosquitto ログ確認

```bash
docker logs mosquitto_broker --follow
```

## wolfSSL 導入メモ

### Python Subscriber での使用

```bash
pip install wolfssl
```

bridge.py は wolfssl が利用可能な場合、自動的に `wolfssl.SSLContext` を優先使用

### C Publisher での使用

Paho MQTT C を wolfSSL でビルド (開発コンテナ環境による)：

```bash
apt-get update && apt-get install -y git build-essential cmake libwolfssl-dev

git clone https://github.com/eclipse/paho.mqtt.c.git
mkdir -p paho.mqtt.c/build && cd paho.mqtt.c/build
cmake .. -DPAHO_WITH_SSL=ON -DWITH_WOLFSSL=ON
make -j && make install
ldconfig
```

> 注：CMake オプションはバージョンにより異なる場合あり

### Mosquitto ブローカー側

公式イメージ `eclipse-mosquitto` は OpenSSL 使用。wolfSSL 版が必要な場合は、ソースからカスタムビルド・イメージ化が必須

## セキュリティに関する注意

### 開発・テスト環境

- 自己署名証明書は全ホスト・ブラウザで警告表示
- `tls_insecure_set(True)` で検証無効化は便宜的コンテキストのみ

### 本番環境への対応

- **証明書**：正規 CA から発行された証明書を使用
- **検証**：`enableServerCertAuth = 1`、`tls_insecure_set(False)` に変更
- **クライアント認証**：双方向認証 (mTLS) の構築検討
