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
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

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

# Set up work directory
WORKDIR /app

# Copy source code
COPY . .

# Build the project
RUN rm -rf build && \
    mkdir -p build && \
    cd build && \
    cmake .. && \
    cmake --build . --target repomix_server -j $(nproc)

# Create logs directory with proper permissions
RUN mkdir -p /app/logs && chmod 777 /app/logs

# Create shared directory for file exchange
RUN mkdir -p /app/shared && chmod 777 /app/shared

# Expose the API port
EXPOSE 9000

# Command to run the server
CMD ["/app/build/bin/repomix_server", "--host", "0.0.0.0", "--port", "9000"] 