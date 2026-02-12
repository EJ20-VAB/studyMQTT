/*
 * MQTT パブリッシャー プログラム
 * 機能：センサーデータをMQTTブローカーへ送信
 * 使用技術：Paho MQTT C Library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "MQTTClient.h"

/* グローバル変数 */
static volatile int stop_signal = 0;
static MQTTClient client;

/* MQTT接続設定 */
#define ADDRESS     "ssl://host.docker.internal:8883" /* MQTTブローカーのアドレス（Mosquitto） - TLS: 8883 */
#define CLIENTID    "C-Publisher-Test"                /* MQTT クライアントID */
#define TOPIC       "sensor/data"                    /* 送信先トピック */
#define QOS         1                                  /* 配信品質：1（最低1回は配信） */
#define TIMEOUT     10000L                             /* タイムアウト：10秒 */
#define SEND_INTERVAL 30                              /* 送信間隔：30秒 */

/*
 * シグナルハンドラー
 * 機能：Ctrl+Cで停止信号を受け取る
 */
void signal_handler(int sig) {
    printf("\n【停止】終了信号を受け取りました。接続を閉じています...\n");
    stop_signal = 1;
}

/*
 * MQTT再接続関数
 * 機能：ブローカーへ再接続
 */
int reconnect_mqtt(MQTTClient* client, MQTTClient_connectOptions* conn_opts) {
    printf("【再接続】ブローカーに再接続を試みています...\n");
    fflush(stdout);
    
    int rc = MQTTClient_connect(*client, conn_opts);
    if (rc == MQTTCLIENT_SUCCESS) {
        printf("【再接続成功】ブローカーに再接続しました。\n");
        fflush(stdout);
        return 0;  /* 成功 */
    } else {
        printf("【再接続失敗】ブローカーへの再接続に失敗しました。リターンコード: %d\n", rc);
        fflush(stdout);
        return -1;  /* 失敗 */
    }
}

/*
 * main関数
 * 処理内容：MQTTクライアントを作成、接続、メッセージ送信、切断
 */
int main(int argc, char* argv[]) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;  /* 接続オプション */
    MQTTClient_message pubmsg = MQTTClient_message_initializer;     /* 送信メッセージ */
    MQTTClient_deliveryToken token;                                 /* メッセージ送信トークン */
    int rc;                                                          /* リターンコード */

    /* Ctrl+Cの停止信号をハンドル */
    signal(SIGINT, signal_handler);

    /* MQTTクライアントを作成（永続メッセージなし） */
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    
    /* 接続オプション設定 */
    conn_opts.keepAliveInterval = 20;  /* キープアライブ間隔：20秒 */
    conn_opts.cleansession = 1;        /* セッションをクリア */

    /* --- TLS/SSL 設定（テスト用：証明書検証無効化） --- */
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    ssl_opts.trustStore = "/work/docker/mosquitto-certs/ca.crt";  /* CA証明書チェーン */
    ssl_opts.enableServerCertAuth = 1;   /* テスト用：証明書検証無効化（本番では=1に） */
    conn_opts.ssl = &ssl_opts;

    /* MQTTブローカーへ接続 */
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* メッセージペイロード作成（実際はセンサーからの取得値などを引数で設定） */
    char* payload = "{\"device_id\": \"C-Client-01\", \"temperature\": 24.5, \"status\": \"OK\"}";
    
    /* メッセージ設定 */
    pubmsg.payload = payload;                           /* ペイロード設定 */
    pubmsg.payloadlen = (int)strlen(payload);           /* ペイロード長 */
    pubmsg.qos = QOS;                                   /* 配信品質 */
    pubmsg.retained = 0;                                /* 保持フラグ（保持しない） */

    printf("【送信開始】メッセージを30秒ごとに送信します。(Ctrl+Cで中断)\n");
    fflush(stdout);

    /* 30秒ごとにメッセージを送信するループ */
    while (!stop_signal) {
        /* MQTTブローカーへメッセージを送信 */
        MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
        /* メッセージ送信待機 */
        printf("【送信】トピック: %s / ペイロード: %s\n", TOPIC, payload);
        fflush(stdout);
        
        rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
        if (rc == MQTTCLIENT_SUCCESS) {
            printf("Message with delivery token %d delivered\n", token);
        } else if (rc == -3) {  /* 接続が切断された場合 */
            printf("Failed to send message, return code %d (接続が切断されています)\n", rc);
            fflush(stdout);
            
            /* 再接続を試みる */
            if (reconnect_mqtt(&client, &conn_opts) != 0) {
                printf("【エラー】再接続に失敗しました。プログラムを終了します。\n");
                fflush(stdout);
                MQTTClient_disconnect(client, 10000);
                MQTTClient_destroy(&client);
                return EXIT_FAILURE;
            }
        } else {
            printf("Failed to send message, return code %d\n", rc);
        }
        fflush(stdout);

        /* 30秒待機（停止信号をチェック） */
        printf("【待機】次の送信まで30秒待機中...\n");
        fflush(stdout);
        if (!stop_signal) {
            sleep(SEND_INTERVAL);
        }
    }

    /* クリーンアップ処理 */
    printf("【クリーンアップ】接続を切断しています...\n");
    fflush(stdout);
    MQTTClient_disconnect(client, 10000);  /* 切断（タイムアウト10秒） */
    MQTTClient_destroy(&client);            /* クライアント破棄(リソース解放) */
    printf("【終了】プログラムを終了しました。\n");
    fflush(stdout);
    return EXIT_SUCCESS;
}