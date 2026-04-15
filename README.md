# Docksmith: A Custom Container Engine

Docksmith is a simplified, Docker-like build and runtime system built entirely from scratch in C++. 

The purpose of this project is to demonstrate a deep understanding of core containerization principles:
1. **Build Caching & Content-Addressing:** Generating deterministic SHA-256 digests for filesystem layers and utilizing a robust build cache.
2. **Process Isolation:** Using OS-level primitives (`chroot`) to sandbox processes without relying on heavy external tools or existing container runtimes (like runc or containerd).
3. **Image Assembly:** Constructing and extracting immutable `.tar` layer deltas to build a unified root filesystem.

## 🏗️ Architecture

Unlike Docker, Docksmith **does not use a background daemon**. It is a single, synchronous CLI binary. All state lives directly on disk in the `~/.docksmith/` directory, which is structured as follows:

* `/images/`: Contains JSON manifests for each built image (defining config and layer order).
* `/layers/`: Contains immutable, content-addressed `.tar` files named by their SHA-256 digest.
* `/cache/`: An index mapping deterministic build instruction hashes to existing layer digests.

## 🛠️ Prerequisites & Installation

Docksmith strictly requires a **Linux** environment (Native, VirtualBox, or WSL2) because it relies on Linux-specific isolation primitives.

### Dependencies
Ensure you have the following installed on your host machine:
* `g++` (Compiler supporting C++17)
* Standard Linux utilities: `tar`, `sha256sum`, `find`, `chroot`
* `nlohmann/json.hpp` (header-only JSON library included in the project directory)

### Build Instructions
1. Clone this repository and navigate into the project folder.
2. Compile the engine using `g++`:
   ```bash
   g++ -std=c++17 main.cpp -o docksmith


TO INSTALL GLOBALLY-
sudo cp docksmith /usr/local/bin/docksmith

Supported Instructions (Docksmithfile)

Docksmith parses a custom Docksmithfile and supports the following 6 instructions:

    FROM <image>: Uses a local base image's layers as the foundation.
    
    WORKDIR <path>: Sets the working directory. Creates it silently if it doesn't exist.

    ENV <key>=<value>: Sets environment variables for the build phase and runtime.

    COPY <src> <dest>: Copies files from the host into the container layer.

    RUN <cmd>: Executes a command strictly isolated inside the container root, saving the delta as a new .tar layer.

    CMD ["exec", "arg"]: Defines the default command to run when the container starts.

 Usage & Commands

    Build an Image: docksmith build -t <name:tag> <context>

    Run a Container: docksmith run [-e KEY=VAL] <name:tag>

    List Images: docksmith images

    Remove Image: docksmith rmi <name:tag>

   Live Demonstration Guide

To verify the core functionalities of Docksmith, follow this standard grading execution flow:
1. Preparation

Ensure you have pulled a base image (e.g., alpine:latest) into your ~/.docksmith/images/ store. Create a script.sh on your host:
   echo 'echo "The container says: $GREETING"' > script.sh
   chmod +x script.sh
Create a Docksmithfile:
   FROM alpine:latest
   WORKDIR /app
   ENV GREETING=Hello
   COPY script.sh /app/script.sh
   RUN chmod +x /app/script.sh
   CMD ["/bin/sh", "/app/script.sh"]
2. The Cold Build (Cache Misses)
   sudo docksmith build -t myapp:latest .
   Markdown

# Docksmith: A Custom Container Engine

Docksmith is a simplified, Docker-like build and runtime system built entirely from scratch in C++. 

The purpose of this project is to demonstrate a deep understanding of core containerization principles:
1. **Build Caching & Content-Addressing:** Generating deterministic SHA-256 digests for filesystem layers and utilizing a robust build cache.
2. **Process Isolation:** Using OS-level primitives (`chroot`) to sandbox processes without relying on heavy external tools or existing container runtimes (like runc or containerd).
3. **Image Assembly:** Constructing and extracting immutable `.tar` layer deltas to build a unified root filesystem.

## 🏗️ Architecture

Unlike Docker, Docksmith **does not use a background daemon**. It is a single, synchronous CLI binary. All state lives directly on disk in the `~/.docksmith/` directory, which is structured as follows:

* `/images/`: Contains JSON manifests for each built image (defining config and layer order).
* `/layers/`: Contains immutable, content-addressed `.tar` files named by their SHA-256 digest.
* `/cache/`: An index mapping deterministic build instruction hashes to existing layer digests.

## 🛠️ Prerequisites & Installation

Docksmith strictly requires a **Linux** environment (Native, VirtualBox, or WSL2) because it relies on Linux-specific isolation primitives.

### Dependencies
Ensure you have the following installed on your host machine:
* `g++` (Compiler supporting C++17)
* Standard Linux utilities: `tar`, `sha256sum`, `find`, `chroot`
* `nlohmann/json.hpp` (header-only JSON library included in the project directory)

### Build Instructions
1. Clone this repository and navigate into the project folder.
2. Compile the engine using `g++`:
   ```bash
   g++ -std=c++17 main.cpp -o docksmith

    Install it globally so it can be run from anywhere:
    Bash

    sudo cp docksmith /usr/local/bin/docksmith

📜 Supported Instructions (Docksmithfile)

Docksmith parses a custom Docksmithfile and supports the following 6 instructions:

    FROM <image>: Uses a local base image's layers as the foundation.

    WORKDIR <path>: Sets the working directory. Creates it silently if it doesn't exist.

    ENV <key>=<value>: Sets environment variables for the build phase and runtime.

    COPY <src> <dest>: Copies files from the host into the container layer.

    RUN <cmd>: Executes a command strictly isolated inside the container root, saving the delta as a new .tar layer.

    CMD ["exec", "arg"]: Defines the default command to run when the container starts.

🚀 Usage & Commands

    Build an Image: docksmith build -t <name:tag> <context>

    Run a Container: docksmith run [-e KEY=VAL] <name:tag>

    List Images: docksmith images

    Remove Image: docksmith rmi <name:tag>

🎬 Live Demonstration Guide

To verify the core functionalities of Docksmith, follow this standard grading execution flow:
1. Preparation

Ensure you have pulled a base image (e.g., alpine:latest) into your ~/.docksmith/images/ store. Create a script.sh on your host:
Bash

echo 'echo "The container says: $GREETING"' > script.sh
chmod +x script.sh

Create a Docksmithfile:
Dockerfile

FROM alpine:latest
WORKDIR /app
ENV GREETING=Hello
COPY script.sh /app/script.sh
RUN chmod +x /app/script.sh
CMD ["/bin/sh", "/app/script.sh"]

2. The Cold Build (Cache Misses)
Bash

sudo docksmith build -t myapp:latest .

Expected: Every layer-producing step (COPY, RUN) will compute file deltas, pack a .tar, and print [CACHE MISS] with the execution time.

3. The Warm Build (Cache Hits)
   sudo docksmith build -t myapp:latest .
   Expected: The engine deterministically hashes the state and realizes no files or commands have changed. It skips execution and instantly prints [CACHE HIT].
   
4. Container Execution & Environment Override
   sudo docksmith run -e GREETING=Welcome myapp:latest
   Expected: The container applies the isolated chroot environment, overrides the ENV variable defined in the image, prints "The container says: Welcome", and exits cleanly.
   
5. Verified Process Isolation (Pass/Fail)
   sudo docksmith run myapp:latest touch /tmp/docksmith_secret_file.txt
   ls -l /tmp/docksmith_secret_file.txt
   Expected: The host system will report No such file or directory. The file was written exclusively inside the container's isolated .run_root namespace, proving strict OS-level      containment.
6.Cleanup
   sudo docksmith rmi myapp:latest

