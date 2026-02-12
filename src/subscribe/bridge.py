# =============================================================================
# AWS Lambda統合 MQTT Subscriber
# =============================================================================
# Mosquittoブローカーからメッセージを受信し、LocalStack上のLambdaを起動
#

# AWS Lambda クライアント操作用
import boto3
# JSON データのシリアライズ・デシリアライズ
import json
# MQTT通信用（Paho MQTT Clientライブラリ）
from paho.mqtt import client as mqtt_client
# TLS/SSL暗号化通信
import ssl
# wolfSSL（暗号化ライブラリ）利用可否の判定
try:
    import wolfssl
    HAVE_WOLFSSL = True
except Exception:
    HAVE_WOLFSSL = False

import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# =============================================================================
# システム設定
# =============================================================================
# LocalStack（AWS ローカル開発環境）のエンドポイントURL
LOCALSTACK_URL = "http://localhost:4566"
# このトピックからMQTTメッセージを受信
TOPIC = "sensor/data"

# =============================================================================
# AWS Lambda クライアント初期化
# =============================================================================
# LocalStack上のLambda APIにアクセスするための設定
lambda_client = boto3.client(
    'lambda', 
    endpoint_url=LOCALSTACK_URL,  # ローカル開発環境のエンドポイント
    region_name='us-east-1',      # AWS リージョン指定
    aws_access_key_id="test",     # LocalStack用の仮想認証キー
    aws_secret_access_key="test"  # LocalStack用の仮想認証キー
)

# =============================================================================
# MQTT メッセージ受信コールバック関数
# =============================================================================
# 機能：受信したMQTTメッセージをログ出力し、Lambdaを非同期で起動

def on_message(client, userdata, msg):
    # MQTT メッセージペイロードをバイト列からUTF-8テキストに変換
    payload = msg.payload.decode()
    # 受信トピック・データをコンソール出力
    print(f"【MQTT受信】トピック: {msg.topic} / データ: {payload}")
    
    # IoT ルール（AWS IoT Coreの自動トリガー機能）の代わりに手動でLambda起動
    try:
        # LocalStack上のLambda関数を同期実行（結果を待機）
        response = lambda_client.invoke(
            FunctionName='iot_processor',       # 実行するLambda関数の名前
            InvocationType='RequestResponse',   # 同期実行モード（結果を待つ）
            Payload=payload                     # MQTT メッセージをペイロードとして渡す
        )
        # Lambda応答をログ出力
        print(f"【Lambda応答確認】")
        # ユーザーへの状態通知
        print(f"MQTT監視中... (トピック: {TOPIC})")
    except Exception as e:
        # Lambda起動失敗時のエラーログ出力
        print(f"【エラー】Lambdaの起動に失敗しました: {e}")


# =============================================================================
# MQTT クライアント初期化・設定
# =============================================================================
# Paho MQTT クライアントの初期化（コールバックAPIを使用）
client = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION2)
# メッセージ受信時のコールバック関数を登録
client.on_message = on_message

# =============================================================================
# TLS/SSL 暗号化通信設定
# =============================================================================
# 通信暗号化の設定
client.tls_set(
    ca_certs=os.path.normpath(os.path.join(BASE_DIR, "../../docker/mosquitto-certs/ca.crt")),                             # CA証明書チェーン
    certfile=os.path.normpath(os.path.join(BASE_DIR, "../../docker/mosquitto-certs/server.crt")),                             # クライアント証明書
    keyfile=os.path.normpath(os.path.join(BASE_DIR, "../../docker/mosquitto-certs/server.key")),                              # クライアント秘密鍵
    cert_reqs=mqtt_client.ssl.CERT_REQUIRED,       # サーバ証明書検証モード
    tls_version=mqtt_client.ssl.PROTOCOL_TLSv1_2,  # TLSバージョン指定
    ciphers=None                               # 暗号スイート（デフォルト使用）
)
# 自己署名証明書の警告を設定
client.tls_insecure_set(False)

# =============================================================================
# Mosquitto ブローカーへ接続・トピック購読・メインループ実行
# =============================================================================
# ホスト経由でブローカーに接続（ホスト・ポート・キープアライブ時間を指定）
client.connect("localhost", 8883, 60)
# 指定したトピックのメッセージ購読開始
client.subscribe(TOPIC)

# ユーザーへの状態通知
print(f"MQTT監視中... (トピック: {TOPIC})")
# メッセージ受信ループを永続実行（接続・受信・コールバックを自動処理）
client.loop_forever()