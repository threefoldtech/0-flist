#!/bin/bash
set -ex


source dependencies.sh

zeroflist() {
    git clone -b development-v2 https://github.com/threefoldtech/0-flist
    pushd 0-flist

    pushd libflist
    make
    popd

    pushd zflist
    make production-mbed
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
libcurl
zeroflist
archive

popd
