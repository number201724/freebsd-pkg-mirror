#!/bin/sh

pkg_dirs=`ls /data/pkg`

for d in $pkg_dirs;do
	cp ./pkg-mirror "/data/pkg/$d"
done
