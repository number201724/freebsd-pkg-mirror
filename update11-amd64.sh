#!/bin/csh



cd /data/pkg-mirror/FreeBSD:11:amd64/latest
./pkg-mirror "http://pkg0.twn.freebsd.org" "FreeBSD:11:amd64" "latest" "/data/pkg/FreeBSD:11:amd64/latest"

cd /data/pkg-mirror/FreeBSD:11:amd64/quarterly
./pkg-mirror "http://pkg0.twn.freebsd.org" "FreeBSD:11:amd64" "quarterly" "/data/pkg/FreeBSD:11:amd64/quarterly"


