FROM ubuntu:19.10
RUN apt-get update
RUN apt-get install -y --no-install-recommends \
    build-essential bison flex libgmp3-dev libmpc-dev \
    libmpfr-dev texinfo wget

WORKDIR /opt/cross/

COPY make-cross-compiler .

RUN ./make-cross-compiler
RUN echo "export PATH=\$PATH:/opt/cross/bin/" >> ~/.bashrc

WORKDIR /opt/src/
