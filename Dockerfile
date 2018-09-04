FROM ubuntu:18.04
RUN apt-get update
RUN apt-get install -y sudo wget
RUN echo "ca_directory=/etc/ssl/certs" > /root/.wgetrc

RUN adduser --disabled-password --gecos '' docker
RUN adduser docker sudo
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER docker
RUN echo "ca_directory=/etc/ssl/certs" > /home/docker/.wgetrc
COPY ./installdeps.sh /tmp
WORKDIR /tmp
RUN ./installdeps.sh

RUN mkdir /tmp/src 
COPY . /tmp/src/
WORKDIR /tmp/src
RUN premake5 gmake2 --include-dir=/usr/include/lua5.3

USER root
WORKDIR /tmp/src
RUN make config=release_static
RUN mkdir -p /app/bin 
RUN cp bin/Static/Release/ckd /app/bin
WORKDIR /app/bin

COPY config.json /app/bin
COPY peers.txt /app/bin 
COPY genesisblock.json /app/bin 
COPY *.lua /app/bin/
RUN cp /usr/lib/lz4.so /app/bin/
CMD ["/app/bin/ckd"]
