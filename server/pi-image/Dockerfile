FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

ARG MIRROR

# Install system tools
RUN if [ "$MIRROR" = "TC" ]; then \
      sed -i 's|http://archive.ubuntu.com/|https://mirrors.cloud.tencent.com/ubuntu/|g' /etc/apt/sources.list.d/ubuntu.sources && \
      sed -i 's|http://security.ubuntu.com/|https://mirrors.cloud.tencent.com/ubuntu/|g' /etc/apt/sources.list.d/ubuntu.sources; \
    fi \
   && apt-get -o Acquire::https::Verify-Peer=false update \
   && apt-get -o Acquire::https::Verify-Peer=false install -y --no-install-recommends \
    ca-certificates \
    curl \
    git \
    ripgrep \
    fd-find \
    procps \
    gnupg \
    jq \
    yq \
    tree \
    less \
    file \
    unzip \
    zip \
    python3-pip \
  && curl -fsSL https://deb.nodesource.com/setup_24.x | bash - \
  && apt-get install -y nodejs

# Install python libraries
RUN if [ "$MIRROR" = "TC" ]; then \
      printf "[global]\nindex-url = https://mirrors.cloud.tencent.com/pypi/simple/\ntrusted-host = mirrors.cloud.tencent.com\n" > /etc/pip.conf; \
    fi \
 && pip3 install --no-cache-dir --break-system-packages \
    numpy~=2.2.0 \
    pandas~=2.2.0 \
    matplotlib~=3.10.0 \
    pillow~=11.3.0 \
    openpyxl==3.1.5 \
    markitdown[all]==0.1.6

# Install nodejs libraries
RUN if [ "$MIRROR" = "TC" ]; then \
      npm config set registry https://mirrors.cloud.tencent.com/npm/; \
    fi \
 && npm install -g @earendil-works/pi-coding-agent@0.79.1

# Create and set workspace
RUN mkdir -p /my && chmod 777 /my
WORKDIR /my

# Clear cache files
RUN apt-get clean \
 && pip3 cache purge \
 && npm cache clean --force

CMD ["bash"]