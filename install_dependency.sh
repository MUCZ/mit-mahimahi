#!/bin/sh 
sudo apt -y update
sudo apt -y upgrade
sudo apt-get install -y dh-autoreconf protobuf-compiler libprotobuf-dev \
autotools-dev iptables pkg-config dnsmasq-base apache2-bin libssl-dev \
ssl-cert debhelper libxcb-present-dev libcairo2-dev libpango1.0-dev \
apache2-dev
