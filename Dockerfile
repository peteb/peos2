FROM docker.pkg.github.com/peteb/peos2/peos-env:latest

ENV PATH /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/cross/bin/

COPY . /opt/src/

RUN build/setenv target make clean all

