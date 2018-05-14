FROM ubuntu:18.04

RUN apt-get update && apt-get install -y \
    make \
    sudo \
    wget

ADD . /srv/CryptoKernel
WORKDIR /srv/CryptoKernel
RUN ./installdeps.sh

ENTRYPOINT ["bash"]
