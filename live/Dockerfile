FROM peos-release:latest

RUN DEBIAN_FRONTEND=noninteractive apt-get install -y net-tools bridge-utils netcat-openbsd iproute2 tshark

COPY qemu-ifup /etc/qemu-ifup
COPY qemu-ifdown /etc/qemu-ifdown
COPY run-live .

CMD ["./run-live"]
