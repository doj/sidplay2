#!/bin/sh

if [ "$1" = clean ] ; then
    make distclean
    rm -rf autom4te.cache configure aclocal.m4
    find . -name Makefile.in -delete
    find . -name '*~' -delete
    find . -name .deps -type d | xargs rm -r
    rm unix/compile unix/config.guess unix/config.sub unix/install-sh unix/missing unix/depcomp unix/ltmain.sh
    exit
fi

set -e
autoreconf --install --symlink --verbose
./configure "$@"
make
