FROM centos:7

# 代理设置（构建时使用，运行容器时可通过 --build-arg 覆盖）
ARG HTTP_PROXY
ARG HTTPS_PROXY
ENV http_proxy=$HTTP_PROXY
ENV https_proxy=$HTTPS_PROXY

# CentOS 7 的镜像源已停止维护，切换到 vault 源
RUN sed -i 's|^mirrorlist=|#mirrorlist=|g' /etc/yum.repos.d/CentOS-*.repo && \
    sed -i 's|^#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*.repo

# 安装基础工具和 SCL 仓库
RUN yum install -y centos-release-scl-rh && \
    sed -i 's|^mirrorlist=|#mirrorlist=|g' /etc/yum.repos.d/CentOS-SCLo-*.repo && \
    sed -i 's|^# *baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-SCLo-*.repo && \
    yum install -y \
        devtoolset-11-gcc-c++ \
        make \
        git \
        curl \
        xz \
        && yum clean all

# SCL 工具链加入 PATH（xmake 编译和后续构建都需要）
ENV PATH="/opt/rh/devtoolset-11/root/usr/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/rh/devtoolset-11/root/usr/lib64:/opt/rh/devtoolset-11/root/usr/lib"
ENV XMAKE_ROOT=y

# 安装 xmake（从源码编译，避免 ncurses.so.6 依赖）
RUN source /opt/rh/devtoolset-11/enable && \
    curl -fsSL https://github.com/xmake-io/xmake/releases/download/v3.0.9/xmake-v3.0.9.tar.gz -o /tmp/xmake.tar.gz && \
    cd /tmp && tar xzf xmake.tar.gz && \
    cd /tmp/xmake-* && \
    ./configure --installdir=/usr/local && \
    make -j$(nproc) && make install && \
    cd / && rm -rf /tmp/xmake.tar.gz /tmp/xmake-*

# 清理代理设置（运行容器时不应继承构建时代理）
ENV http_proxy=""
ENV https_proxy=""

WORKDIR /workspace
