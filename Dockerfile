FROM ubuntu:22.04

# Avoid prompts from apt
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libjsoncpp-dev \
    libssl-dev \
    uuid-dev \
    zlib1g-dev \
    libc-ares-dev \
    curl \
    libcurl4-openssl-dev \
    libbrotli-dev \
    libpq-dev \
    libmysqlclient-dev \
    libsqlite3-dev \
    libhiredis-dev \
    libfmt-dev \
    libpcre2-dev \
    libicu-dev \
    wget \
    python3 \
    nodejs \
    npm \
    libtree-sitter-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Determine Tree-sitter include and library paths
RUN mkdir -p /tmp/tree-sitter-config && \
    echo "Locating Tree-sitter files:" && \
    find /usr -name "tree_sitter" -type d | tee /tmp/tree-sitter-config/include_paths.txt && \
    find /usr -name "api.h" | grep tree_sitter | tee /tmp/tree-sitter-config/api_paths.txt && \
    find /usr -name "libtree-sitter.so*" | tee /tmp/tree-sitter-config/lib_paths.txt

# Build and install Drogon
RUN git clone --recursive --depth=1 --branch v1.9.10 https://github.com/drogonframework/drogon.git /tmp/drogon && \
    cd /tmp/drogon && \
    mkdir build && \
    cd build && \
    cmake .. -DBUILD_EXAMPLES=OFF -DBUILD_CTL=OFF -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd / && \
    rm -rf /tmp/drogon

# Install fmt library if not already installed by package
RUN if [ ! -f "/usr/include/fmt/core.h" ]; then \
    git clone https://github.com/fmtlib/fmt.git /tmp/fmt && \
    cd /tmp/fmt && \
    mkdir build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DFMT_TEST=OFF && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd / && \
    rm -rf /tmp/fmt; \
fi

# Clone and build cpp-tiktoken
RUN git clone --recursive https://github.com/gh-markt/cpp-tiktoken.git /usr/local/src/cpp-tiktoken && \
    cd /usr/local/src/cpp-tiktoken && \
    # Make sure the PCRE2 submodule is properly initialized
    git submodule update --init --recursive && \
    mkdir -p build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DPCRE2_INCLUDE_DIR=/usr/include -DPCRE2_LIBRARY=/usr/lib/$(uname -m)-linux-gnu/libpcre2-8.so && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Download and install ONNX Runtime with architecture detection
RUN mkdir -p /usr/local/onnxruntime && \
    cd /usr/local/onnxruntime && \
    ARCH=$(uname -m) && \
    ONNXRUNTIME_VERSION="1.15.1" && \
    if [ "$ARCH" = "x86_64" ]; then \
        ONNX_RUNTIME_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz"; \
    elif [ "$ARCH" = "aarch64" ]; then \
        ONNX_RUNTIME_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-aarch64-${ONNXRUNTIME_VERSION}.tgz"; \
    else \
        echo "Architecture not supported for ONNX Runtime" && exit 1; \
    fi && \
    echo "Downloading ONNX Runtime from $ONNX_RUNTIME_URL" && \
    curl -L ${ONNX_RUNTIME_URL} -o onnxruntime.tgz && \
    tar -xzf onnxruntime.tgz --strip-components=1 && \
    rm onnxruntime.tgz && \
    # Create symlinks for libraries to be found during build
    ln -s /usr/local/onnxruntime/lib/libonnxruntime.so /usr/local/lib/libonnxruntime.so && \
    ln -s /usr/local/onnxruntime/lib/libonnxruntime.so.1.15.1 /usr/local/lib/libonnxruntime.so.1.15.1 && \
    # Add include path to standard includes
    cp -r /usr/local/onnxruntime/include/* /usr/local/include/ && \
    ldconfig

# Create model directory with placeholder files if needed
RUN mkdir -p /app/models && \
    cd /app/models && \
    # Try to download the models but continue even if it fails
    if ! wget -q --timeout=30 --tries=3 https://huggingface.co/microsoft/codebert-base/resolve/main/onnx/model.onnx -O codebert-ner.onnx; then \
        # Create empty model file as a placeholder if download fails
        echo "Failed to download model, creating placeholder" && \
        touch codebert-ner.onnx; \
    fi && \
    if ! wget -q --timeout=30 --tries=3 https://huggingface.co/microsoft/codebert-base/resolve/main/vocab.json -O vocab.txt; then \
        # Create basic vocab file as a placeholder if download fails
        echo "Failed to download vocab, creating placeholder" && \
        echo '{"placeholder": 0}' > vocab.txt; \
    fi

# Set up work directory
WORKDIR /app

# Create directory for tree-sitter repositories
RUN mkdir -p /app/external/tree-sitter-repos

# Copy source code
COPY . .

# Copy tree-sitter repositories if they exist locally
COPY external/tree-sitter-repos/ /app/external/tree-sitter-repos/

# Build the project
RUN rm -rf build && \
    mkdir -p build && \
    cd build && \
    # Set Tree-sitter variables based on find results
    TS_LIB=$(cat /tmp/tree-sitter-config/lib_paths.txt | head -n 1) && \
    TS_INCLUDE_DIR=$(dirname $(cat /tmp/tree-sitter-config/api_paths.txt | head -n 1)) && \
    echo "Using Tree-sitter lib: ${TS_LIB}" && \
    echo "Using Tree-sitter include: ${TS_INCLUDE_DIR}" && \
    cmake .. \
      -DONNX_RUNTIME_LIB=/usr/local/lib/libonnxruntime.so \
      -DONNX_RUNTIME_INCLUDE_DIR=/usr/local/include \
      -DTREE_SITTER_LIB="${TS_LIB}" \
      -DTREE_SITTER_INCLUDE_DIR="${TS_INCLUDE_DIR}" \
      -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --target repomix -j $(nproc) && \
    cmake --build . --target repomix_server -j $(nproc)

# Create logs directory with proper permissions
RUN mkdir -p /app/logs && chmod 777 /app/logs

# Create shared directory for file exchange
RUN mkdir -p /app/shared && chmod 777 /app/shared

# Expose the API port
EXPOSE 9000

# Command to run the server
CMD ["/app/build/bin/repomix_server", "--host", "0.0.0.0", "--port", "9000"] 