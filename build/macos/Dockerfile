FROM ubuntu:18.04

RUN apt update && apt dist-upgrade -y && apt install -y clang git

RUN git clone https://github.com/tpoechtrager/osxcross.git
WORKDIR osxcross

ADD Xcode_7.3.dmg .
RUN ./tools/gen_sdk_package_p7zip.sh Xcode_7.3.dmg

RUN rm Xcode_7.3.dmg


