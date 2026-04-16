# Docksmith

Docksmith is a simplified, from-scratch container engine and build system. [cite_start]It implements core containerization concepts including layered filesystems, content-addressable storage, deterministic build caching, and process isolation using Linux primitives[cite: 3, 4].

## 🚀 Essential Setup (Required for Demo)

[cite_start]Docksmith is designed to work fully offline. Before building any images, you must manually import the Alpine base image into the local store.

1. **Download the Alpine RootFS:**
   ```bash
   wget [https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz](https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz)

Initialize Docksmith Directories:
   mkdir -p ~/.docksmith/images ~/.docksmith/layers ~/.docksmith/cache
   
Import the Base Layer:
   mv alpine-minirootfs-3.20.3-x86_64.tar.gz ~/.docksmith/layers/sha256:alpine-base

Create the Base Image Manifest:
Save the following as ~/.docksmith/images/alpine.json:
   {
    "name": "alpine",
    "tag": "latest",
    "layers": [{"digest": "sha256:alpine-base"}],
    "config": {
        "Env": [],
        "Cmd": ["/bin/sh"],
        "WorkingDir": "/"
    },
    "digest": "sha256:alpine-base-placeholder"
}

🛠 Features

    Layered Architecture: COPY and RUN instructions produce immutable delta layers.

    Deterministic Caching: Build steps use a complex hashing key to provide [CACHE HIT] or [CACHE MISS] statuses.

    Process Isolation: Uses chroot to ensure container processes cannot access the host filesystem.

    Metadata Management: Image configurations (ENV, WORKDIR, CMD) are stored in JSON manifests.

💻 Usage
Build an Image
   sudo ./docksmith build -t myapp:latest .
Run a Container
   sudo ./docksmith run myapp:latest
Run with Environment Overrides
   sudo ./docksmith run -e GREETING=Welcome myapp:latest
List Images
   ./docksmith images


   
