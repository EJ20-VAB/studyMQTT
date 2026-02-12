# ====================================================
# AWS Provider 設定
# LocalStackを使用して、ローカルでAWSリソースをエミュレート
# ====================================================
provider "aws" {
  access_key                  = "test"
  secret_key                  = "test"
  region                      = var.aws_region

  # LocalStackを使用するための設定
  s3_use_path_style           = true
  skip_credentials_validation = true
  skip_metadata_api_check     = true
  skip_requesting_account_id  = true

  # LocalStackのエンドポイントを指定
  endpoints {
    ec2    = var.localstack_endpoint
    iam    = var.localstack_endpoint
    lambda = var.localstack_endpoint
    iot    = var.localstack_endpoint
    sts    = var.localstack_endpoint
  }
}

# ====================================================
# AWS EC2 インスタンス
# 機能：ダミーサーバーをEC2から起動
# ====================================================
resource "aws_instance" "dummy_server" {
  ami           = var.ami_id
  instance_type = var.instance_type

  tags = {
    Name = "LocalStack-EC2-Test"
  }
}

# ====================================================
# RDS MySQL ダミーリソース
# 機能：MySQL DBがプロビジョンされたことを転走
# ====================================================
resource "null_resource" "dummy_mysql" {
  provisioner "local-exec" {
    command = "echo 'MySQL DB has been provisioned. Host: ${var.localstack_endpoint}, Port: 3306'"
  }
}

# ====================================================
# 出力：DB接続情報
# ====================================================
output "db_info" {
  value = "Database is ready at ${var.localstack_endpoint}"
}

# --- AWS IoT Core (Topic Rule) ---
# MQTTトピックにメッセージが来たらLambdaを起動する設定
/* # 無料版では動かないためコメントアウト
resource "aws_iot_topic_rule" "iot_to_lambda" {
  name        = "IotToLambdaRule"
  description = "Send MQTT messages to Lambda"
  enabled     = true
  sql         = "SELECT * FROM 'sensor/data'" # 監視するMQTTトピック
  sql_version = "2016-03-23"

  lambda {
    function_arn = aws_lambda_function.iot_processor.arn
  }
}
*/

# --- AWS Lambda (Python) ---
resource "aws_lambda_function" "iot_processor" {
  filename      = "lambda_function.zip"
  function_name = "iot_processor"
  role          = aws_iam_role.lambda_role.arn
  handler       = "index.lambda_handler"
  runtime       = "python3.9"

  # Lambda実行時のDB接続情報を環境変数で設定
  environment {
    variables = {
      DB_HOST = "localhost" # 実際はEC2上にデプロイされたMySQL等を指す
    }
  }
}

# ====================================================
# IAM Role for Lambda
# 機能：Lambda関数がAWSリソースを使用するための権限を付与
# 控制：LocalStackでは粗い権限でも定義が必須
# ====================================================
resource "aws_iam_role" "lambda_role" {
  name = "lambda_role"
  # Lambdaサービスがこのロールを引き受ける権限を付与
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{ 
      Action = "sts:AssumeRole", 
      Effect = "Allow", 
      Principal = { Service = "lambda.amazonaws.com" } 
    }]
  })
}

# ====================================================
# IAM Policy - Lambda Permission
# 機能：IoT CoreかLambda関数を起動する権限を設定
# ====================================================
resource "aws_lambda_permission" "allow_iot" {
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.iot_processor.function_name
  principal     = "iot.amazonaws.com"  # IoTコアからの起動を許可
}