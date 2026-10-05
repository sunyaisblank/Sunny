# Linux/amd64 release inputs are locked in release/build-inputs.json. Developer
# CMake builds retain their normal system-package/fallback behavior. Produce a
# reviewable offline release with tools/release.py; it records a clean commit.
FROM ubuntu:24.04@sha256:f610ab94648195aa356059f5b41d6085c9d4d903c072430cdd1af7bdb646106b AS builder

ARG SUNNY_APT_SNAPSHOT=20261001T000000Z
ARG SUNNY_CA_CERTIFICATES_URL=https://snapshot.ubuntu.com/ubuntu/20261001T000000Z/pool/main/c/ca-certificates/ca-certificates_20260601~24.04.1_all.deb
ARG SUNNY_CA_CERTIFICATES_SHA256=6bac2a01979e210d9eac1d4d56747ec709ea60654744d66705dc3c36e7629e50
ARG SUNNY_BUILD_PACKAGES="ca-certificates=20260601~24.04.1 cmake=3.28.3-1build7 g++=4:13.2.0-7ubuntu1 git=1:2.43.0-1ubuntu7.3 ninja-build=1.11.1-2 python3=3.12.3-0ubuntu2.1"
ARG SUNNY_SOURCE_REVISION=unrecorded-development-build
ARG SOURCE_DATE_EPOCH=0

ADD --checksum=sha256:${SUNNY_CA_CERTIFICATES_SHA256} ${SUNNY_CA_CERTIFICATES_URL} /tmp/sunny-ca-certificates.deb
COPY tools/apt_snapshot.sh /tmp/sunny-apt-snapshot.sh
RUN sh /tmp/sunny-apt-snapshot.sh "$SUNNY_APT_SNAPSHOT" "$SUNNY_BUILD_PACKAGES" /builder-apt-metadata.txt "$SUNNY_CA_CERTIFICATES_SHA256" && \
    dpkg-query -W -f='${Package}\t${Version}\t${Architecture}\n' > /builder-packages.tsv && \
    rm /tmp/sunny-apt-snapshot.sh

WORKDIR /build
COPY CMakeLists.txt CMakePresets.json pyproject.toml ./
COPY apps ./apps
COPY cmake ./cmake
COPY include ./include
COPY src ./src
COPY release/build-inputs.json ./release/build-inputs.json
COPY tools/release.py ./tools/release.py
COPY tools/doctor.py ./tools/doctor.py
COPY tools/live_qualification ./tools/live_qualification
COPY tools/windows ./tools/windows
COPY remote_script/Sunny ./remote_script/Sunny
COPY python/sunny/__init__.py ./python/sunny/__init__.py
COPY max-package/CMakeLists.txt max-package/package-info.json ./max-package/

RUN cmake -B .bin -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DSUNNY_RELEASE_BUILD=ON \
        -DSUNNY_BUILD_TESTS=OFF \
        -DSUNNY_BUILD_PYTHON_BINDINGS=OFF && \
    cmake --build .bin --target sunny-mcp --parallel 2 && \
    python3 tools/release.py record-build \
        --source-revision "$SUNNY_SOURCE_REVISION" \
        --source-date-epoch "$SOURCE_DATE_EPOCH" \
        --build-directory .bin --packages /builder-packages.tsv \
        --apt-metadata /builder-apt-metadata.txt \
        --output .bin/release

FROM ubuntu:24.04@sha256:f610ab94648195aa356059f5b41d6085c9d4d903c072430cdd1af7bdb646106b AS runtime

ARG SUNNY_APT_SNAPSHOT=20261001T000000Z
ARG SUNNY_CA_CERTIFICATES_URL=https://snapshot.ubuntu.com/ubuntu/20261001T000000Z/pool/main/c/ca-certificates/ca-certificates_20260601~24.04.1_all.deb
ARG SUNNY_CA_CERTIFICATES_SHA256=6bac2a01979e210d9eac1d4d56747ec709ea60654744d66705dc3c36e7629e50
ARG SUNNY_RUNTIME_PACKAGES="libstdc++6=14.2.0-4ubuntu2~24.04.1 python3=3.12.3-0ubuntu2.1"
ARG SUNNY_SOURCE_REVISION=unrecorded-development-build
ARG SUNNY_VERSION=0.4.0
ARG SUNNY_BUILD_INPUTS_SHA256

ADD --checksum=sha256:${SUNNY_CA_CERTIFICATES_SHA256} ${SUNNY_CA_CERTIFICATES_URL} /tmp/sunny-ca-certificates.deb
COPY tools/apt_snapshot.sh /tmp/sunny-apt-snapshot.sh
RUN sh /tmp/sunny-apt-snapshot.sh "$SUNNY_APT_SNAPSHOT" "$SUNNY_RUNTIME_PACKAGES" /runtime-apt-metadata.txt "$SUNNY_CA_CERTIFICATES_SHA256" && \
    mkdir -p /opt/sunny/release && \
    dpkg-query -W -f='${Package}\t${Version}\t${Architecture}\n' > /opt/sunny/release/runtime-packages.tsv && \
    mv /runtime-apt-metadata.txt /opt/sunny/release/runtime-apt-metadata.txt && \
    rm /tmp/sunny-apt-snapshot.sh && \
    groupadd --gid 1001 sunny && \
    useradd --uid 1001 --gid 1001 --create-home --shell /bin/bash sunny && \
    mkdir /data && chown sunny:sunny /data

COPY --from=builder /build/.bin/sunny-mcp /usr/local/bin/sunny-mcp
COPY --from=builder /build/.bin/remote_script/Sunny /opt/sunny/remote-script/Sunny
COPY --from=builder /build/.bin/release/ /opt/sunny/release/
COPY --from=builder /build/tools/windows/ /opt/sunny/installer/windows/
COPY --from=builder /build/tools/doctor.py /opt/sunny/operator/doctor.py
COPY --from=builder /build/tools/release.py /opt/sunny/operator/release.py
COPY --from=builder /build/tools/live_qualification/ /opt/sunny/operator/live_qualification/
COPY README.md /opt/sunny/operator/README.md

USER sunny
ENTRYPOINT ["sunny-mcp"]

LABEL org.opencontainers.image.title="Sunny" \
      org.opencontainers.image.description="Music theory MCP server with Ableton Live integration" \
      org.opencontainers.image.vendor="Sunny Project" \
      org.opencontainers.image.source="https://github.com/sunyaisblank/Sunny" \
      org.opencontainers.image.version="$SUNNY_VERSION" \
      org.opencontainers.image.revision="$SUNNY_SOURCE_REVISION" \
      org.opencontainers.image.base.name="ubuntu:24.04" \
      org.opencontainers.image.base.digest="sha256:f610ab94648195aa356059f5b41d6085c9d4d903c072430cdd1af7bdb646106b" \
      org.sunny.build-inputs.sha256="$SUNNY_BUILD_INPUTS_SHA256" \
      org.sunny.configuration.contract="versioned_json" \
      org.sunny.configuration.schema-version="1"
