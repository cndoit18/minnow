FROM ubuntu:jammy

RUN apt-get update \
    && apt-get -y install lsb-release software-properties-common wget sudo dialog \
    && add-apt-repository -y multiverse \
    && add-apt-repository -y universe \
    && add-apt-repository -y restricted \
    && wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null \
    && apt-add-repository -y "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" \
    && wget --no-check-certificate -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - \
    && add-apt-repository -y "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs) main" \
    && apt-get update \
    && apt-get -y dist-upgrade

RUN echo "wireshark-common wireshark-common/install-setuid boolean true" | debconf-set-selections      \
    && DEBIAN_FRONTEND=noninteractive apt-get -y install build-essential gdb gcc gcc-12 g++ g++-12 cmake \
        libpcap-dev htop jnettop screen emacs-nox vim-nox automake pkg-config libtool libtool-bin git  \
        tig links parallel iptables mininet net-tools tcpdump wireshark telnet socat clang clang-15 \
        clang-format clang-format-15 clang-tidy clang-tidy-15 coreutils bash doxygen graphviz \
        netcat-openbsd clangd clangd-15

RUN for ver in 11 12; do                                                                      \
        update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${ver} $((10 * ${ver})) \
            $(for prog in g++ gcc-ar gcc-nm gcc-ranlib gcov gcov-dump gcov-tool; do         \
                echo "--slave /usr/bin/${prog} ${prog} /usr/bin/${prog}-${ver}";             \
            done);                                                                           \
    done

RUN for ver in 14 15; do                                                                      \
        update-alternatives --install /usr/bin/clang clang /usr/bin/clang-${ver} $((10 * ${ver})) \
            $(for prog in clang-format clang-tidy clangd; do         \
                echo "--slave /usr/bin/${prog} ${prog} /usr/bin/${prog}-${ver}";             \
            done);                                                                           \
    done

RUN adduser --disabled-password --gecos '' docker
RUN adduser docker sudo
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER docker