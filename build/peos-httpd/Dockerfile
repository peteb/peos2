FROM peos-release:latest

RUN DEBIAN_FRONTEND=noninteractive apt-get install -y net-tools bridge-utils netcat iproute2 tshark emacs

COPY qemu-ifup /etc/qemu-ifup
COPY qemu-ifdown /etc/qemu-ifdown

CMD ["./run-qemu", "httpd-tap"]
