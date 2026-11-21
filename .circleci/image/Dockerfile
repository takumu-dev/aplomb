# The image every Linux job of aplomb's CI runs in, for x86-64 and ARM64 alike: the floor compilers,
# GCC 16 and Clang 23 with libstdc++, clang-format and clang-tidy, and the tools a preset needs,
# with the headers of the X11 and OpenGL libraries that the sandbox's window is built against. No
# job opens a window.
#
# It is published as ghcr.io/takumu-dev/aplomb-ci, tagged with the git tree hash of this directory,
# and built only when that tag is missing. Any change here is a new image under a new tag, so a
# distribution update reaches a build only through a commit.
FROM gcc:16.2.0-trixie@sha256:b8a42d6804296fbe7a54e17e9d60c3100d9e8dbe7ef88c748cae2ddb00d63ba2

ARG LLVM_VERSION=23
ARG CMAKE_VERSION=4.4.5
ARG CMAKE_SHA256_X86_64=ed85135c5a5135b45e50423cb406dea04ca3338da0b8ef3c9ede963559680be0
ARG CMAKE_SHA256_AARCH64=301d5063ab259577c1b57ce5490b1aab9ea5c56dc4c822f73499553dab94925f
ARG VCPKG_TAG=2026.10.16

LABEL org.opencontainers.image.source="https://github.com/takumu-dev/aplomb"
LABEL org.opencontainers.image.description="The floor compilers and tools of aplomb's Linux CI"
LABEL org.opencontainers.image.licenses="MIT"

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Clang from LLVM's own packages, since Debian's stop short of the floor.
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl gnupg \
    && curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key \
        | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] https://apt.llvm.org/trixie/" \
        "llvm-toolchain-trixie-${LLVM_VERSION} main" > /etc/apt/sources.list.d/llvm.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        clang-${LLVM_VERSION} \
        clang-format-${LLVM_VERSION} \
        clang-tidy-${LLVM_VERSION} \
        libclang-rt-${LLVM_VERSION}-dev \
        ccache \
        git \
        jq \
        libgl-dev \
        libx11-dev \
        libxcursor-dev \
        libxi-dev \
        libxinerama-dev \
        libxrandr-dev \
        ninja-build \
        pkg-config \
        tar \
        unzip \
        zip \
    && rm -rf /var/lib/apt/lists/* \
    && for tool in clang clang++ clang-format clang-tidy run-clang-tidy; do \
        ln -s "/usr/bin/${tool}-${LLVM_VERSION}" "/usr/local/bin/${tool}"; \
    done

# The base image's GCC 16 lives in /usr/local, beside Debian's own older GCC, which Clang would
# otherwise pick for its libstdc++. A configuration file beside Clang's binaries points it, and
# clang-tidy through it, at GCC 16 instead.
RUN gcc_dir="$(dirname "$(gcc -print-libgcc-file-name)")" \
    && for driver in clang clang++; do \
        echo "--gcc-install-dir=${gcc_dir}" > "/usr/lib/llvm-${LLVM_VERSION}/bin/${driver}.cfg"; \
    done

# CMake from Kitware's releases, since Debian's stops short of the floor.
RUN arch="$(uname -m)" \
    && case "${arch}" in \
        x86_64) sha256="${CMAKE_SHA256_X86_64}" ;; \
        aarch64) sha256="${CMAKE_SHA256_AARCH64}" ;; \
        *) echo "no CMake for ${arch}" >&2; exit 1 ;; \
    esac \
    && archive="cmake-${CMAKE_VERSION}-linux-${arch}.tar.gz" \
    && curl -fsSLO "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/${archive}" \
    && echo "${sha256}  ${archive}" | sha256sum -c - \
    && tar -xzf "${archive}" -C /usr/local --strip-components=1 \
    && rm "${archive}"

# vcpkg's tool. A job checks its registry out at the manifest's baseline before it configures, so
# the baseline moves without a new image. vcpkg reads older versions of ports from the registry's
# history, so the clone keeps every commit and tree, and fetches the files of a port only when it
# needs them. vcpkg uses the CMake and Ninja above rather than downloading its own, which it has
# none of for ARM64.
ENV VCPKG_ROOT=/opt/vcpkg \
    VCPKG_FORCE_SYSTEM_BINARIES=1 \
    VCPKG_DISABLE_METRICS=1
RUN git clone --filter=blob:none --branch "${VCPKG_TAG}" https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}" \
    && "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics \
    && ln -s "${VCPKG_ROOT}/vcpkg" /usr/local/bin/vcpkg

# Fails the build if a tool is missing, or if Clang or clang-tidy would take any libstdc++ but
# GCC 16's.
RUN g++ --version && clang++ --version && clang-format --version && clang-tidy --version \
    && cmake --version && ninja --version && ccache --version && vcpkg version \
    && clang++ -v 2>&1 | grep -E "Selected GCC installation: .*/gcc/[^/]+/16\." \
    && echo 'int main() {}' > /tmp/main.cpp \
    && clang-tidy --checks=-*,misc-unused-alias-decls /tmp/main.cpp --extra-arg=-v -- 2>&1 \
        | grep -E "Selected GCC installation: .*/gcc/[^/]+/16\." \
    && rm /tmp/main.cpp
