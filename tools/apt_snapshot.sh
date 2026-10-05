#!/bin/sh
# Bootstrap only the locked CA bytes, then use one signed immutable snapshot.
set -eu
set -f

snapshot=$1
packages=$2
metadata=$3
bootstrap_sha256=$4

printf '%s  /tmp/sunny-ca-certificates.deb\n' "$bootstrap_sha256" | sha256sum --check -
dpkg-deb --extract /tmp/sunny-ca-certificates.deb /tmp/sunny-ca-certificates
find /tmp/sunny-ca-certificates/usr/share/ca-certificates -type f -name '*.crt' \
    -exec cat {} + > /tmp/sunny-apt-ca.pem
printf 'Types: deb\nURIs: https://snapshot.ubuntu.com/ubuntu/%s/\nSuites: noble noble-updates noble-security\nComponents: main universe\nSigned-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n' \
    "$snapshot" > /etc/apt/sources.list.d/ubuntu.sources
rm -rf /var/lib/apt/lists/*
apt-get -o Acquire::https::CaInfo=/tmp/sunny-apt-ca.pem -o APT::Update::Error-Mode=any update
# Word splitting is intentional for the exact, space-separated roots from the
# committed lock. Pattern-Only disables implicit regex fallback for names such
# as g++; an unavailable exact package must fail rather than select other names.
apt-get -o Acquire::https::CaInfo=/tmp/sunny-apt-ca.pem -o APT::Cmd::Pattern-Only=true \
    install -y --no-install-recommends $packages
apt-cache -o APT::Cmd::Pattern-Only=true show $packages > "$metadata"
rm -rf /tmp/sunny-ca-certificates.deb /tmp/sunny-ca-certificates /tmp/sunny-apt-ca.pem \
    /var/lib/apt/lists/*
