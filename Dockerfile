FROM docker.pkg.github.com/peteb/peos2/peos-toolchain:latest

ENV PATH /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/cross/bin/
ENV DEFS -DNODEBUG

COPY . /opt/src/

RUN setenv target make clean all
