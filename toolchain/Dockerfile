FROM ubuntu:19.10
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -yq --no-install-recommends \
    build-essential bison flex libgmp3-dev libmpc-dev \
    libmpfr-dev texinfo wget grub-pc-bin xorriso mtools \
    qemu-kvm expect ruby netcat-openbsd bridge-utils net-tools \
    iproute2 tshark gdb

WORKDIR /opt/cross/

COPY make-cross-compiler .
RUN ./make-cross-compiler

COPY make-bochs .
RUN ./make-bochs

COPY setenv /opt/cross/bin

RUN echo "export PATH=\$PATH:/opt/cross/bin/" >> ~/.bashrc && chmod +x ~/.bashrc

WORKDIR /opt/src/
