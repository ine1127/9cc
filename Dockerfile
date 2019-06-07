FROM debian:stretch
LABEL maintainer <ine1127@example.com>

RUN apt update      \
 && apt upgrade -y  \
 && apt install -y  \
        binutils    \
        git         \
        gcc         \
        gdb         \
        libc6-dev   \
        make        \
 && :

WORKDIR "/root"

VOLUME ["/root"]

CMD ["/bin/bash"]
