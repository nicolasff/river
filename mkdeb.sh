#!/bin/sh
make clean all
rm -rf debian

mkdir -p debian/DEBIAN/
mkdir -p debian/etc/init.d/
mkdir -p debian/usr/bin/

cp river.conf debian/etc/river.conf
cp river debian/usr/bin
cp init-script debian/etc/init.d/river


cp debian.control debian/DEBIAN/control
dpkg -b debian river-`uname -m`.deb
rm -rf debian/
