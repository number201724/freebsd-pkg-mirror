#!/bin/csh


cd /data/pkg-mirror/FreeBSD:11:i386/latest
./pkg-mirror "http://pkg0.twn.freebsd.org" "FreeBSD:11:i386" "latest" "/data/pkg/FreeBSD:11:i386/latest"

cd /data/pkg-mirror/FreeBSD:11:i386/quarterly
./pkg-mirror "http://pkg0.twn.freebsd.org" "FreeBSD:11:i386" "quarterly" "/data/pkg/FreeBSD:11:i386/quarterly"


