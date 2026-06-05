FROM ubuntu:25.10

ENV DEBIAN_FRONTEND=noninteractive

COPY ubuntu.sources /etc/apt/sources.list.d/ubuntu.sources

RUN apt-get -o "Acquire::https::Verify-Peer=false" -o "Acquire::AllowInsecureRepositories=true" -o "Acquire::AllowDowngradeToInsecureRepositories=true" update && \
    apt-get -o "Acquire::https::Verify-Peer=false" -o "Acquire::AllowInsecureRepositories=true" -o "Acquire::AllowDowngradeToInsecureRepositories=true" install -y aria2 curl ca-certificates --no-install-recommends && \
    rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y \
    build-essential libncurses-dev libelf-dev \
    bash binutils git make ninja-build \
    flex bison meson clangd \
    gcc-arm-none-eabi gdb-multiarch  \
    picolibc-arm-none-eabi libstdc++-arm-none-eabi-picolibc \
    just unzip wget rsync file cpio fish locales sudo --no-install-recommends\
    && rm -rf /var/lib/apt/lists/*

RUN ln -s /usr/bin/gdb-multiarch /usr/bin/arm-none-eabi-gdb

RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8

RUN echo "ubuntu ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN echo $(which fish) >> /etc/shells && \
    chsh -s $(which fish) ubuntu

USER ubuntu

RUN mkdir -p ~/.config/fish/completions && \
    just --completions fish > ~/.config/fish/completions/just.fish
