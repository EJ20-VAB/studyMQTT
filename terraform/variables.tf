variable "aws_region" {
  description = "AWS region"
  default     = "us-east-1"
}

variable "localstack_endpoint" {
  description = "LocalStack endpoint URL"
  default     = "http://localhost:4566"
}

variable "instance_type" {
  description = "EC2 instance type"
  default     = "t2.micro"
}

variable "ami_id" {
  description = "AMI ID for the instance"
  default     = "ami-005e54dee72cc1d00"
}