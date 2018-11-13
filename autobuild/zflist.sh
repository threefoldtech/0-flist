#!/bin/bash
set -ex

makeopts="-j 5"

dependencies() {
    apt-get update

    apt-get install -y build-essential git libsnappy-dev libz-dev \
        libtar-dev libb2-dev autoconf libtool libjansson-dev \
        libhiredis-dev libsqlite3-dev libcurl4-openssl-dev
}

capnp() {
    git clone https://github.com/opensourcerouting/c-capnproto
    pushd c-capnproto
    git submodule update --init --recursive
    autoreconf -f -i -s

    ./configure
    make ${makeopts}
    make install
    ldconfig

    popd
}

zeroflist() {
    git clone -b development https://github.com/threefoldtech/0-flist
    pushd 0-flist

    pushd libflist
    make
    popd

    pushd zflist
    make production
    popd

    cp zflist/zflist /tmp/zflist
    strip -s /tmp/zflist

    popd
}

archive() {
    mkdir -p /tmp/archives
    tar -czvf /tmp/archives/zflist.tar.gz -C /tmp/ zflist
}

pushd /opt

dependencies
capnp
zeroflist
archive

popd
