#!/bin/csh


cd /data/pkg-mirror/FreeBSD:13:i386/latest
./pkg-mirror "http://pkg0.twn.freebsd.org" "FreeBSD:13:i386" "latest" "/data/pkg/FreeBSD:13:i386/latest"

#cd /root/pkg/FreeBSD:13:i386/quarterly
#./pkg-mirror "http://pkg0.kwc.freebsd.org" "FreeBSD:13:i386" "quarterly" "/data/pkg/FreeBSD:13:i386/quarterly"

cd /data/pkg-mirror/FreeBSD:13:amd64/latest
./pkg-mirror "http://pkg0.twn.freebsd.org" "FreeBSD:13:amd64" "latest" "/data/pkg/FreeBSD:13:amd64/latest"

#cd /root/pkg/FreeBSD:13:amd64/quarterly
#./pkg-mirror "http://pkg0.kwc.freebsd.org" "FreeBSD:13:amd64" "quarterly" "/data/pkg/FreeBSD:13:amd64/quarterly"

cd /data/pkg-mirror/FreeBSD:13:aarch64/latest
./pkg-mirror "http://pkg0.twn.freebsd.org" "FreeBSD:13:aarch64" "latest" "/data/pkg/FreeBSD:13:aarch64/latest"

#cd /root/pkg/FreeBSD:13:aarch64/quarterly
#./pkg-mirror "http://pkg0.kwc.freebsd.org" "FreeBSD:13:aarch64" "quarterly" "/data/pkg/FreeBSD:13:aarch64/quarterly"
