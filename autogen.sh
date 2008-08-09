#!/bin/sh

autoreconf -v --install || exit $?
./configure "$@"
