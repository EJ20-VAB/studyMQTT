FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /work

RUN apt-get update && apt-get install -y \
    build-essential cmake git pkg-config wget ca-certificates \
    libwolfssl-dev libcurl4-openssl-dev libssl-dev \
    gcc g++ make autoconf automake libtool \
    && rm -rf /var/lib/apt/lists/*

COPY build_and_package.sh /work/build_and_package.sh
RUN chmod +x /work/build_and_package.sh

ENTRYPOINT ["/work/build_and_package.sh"]
