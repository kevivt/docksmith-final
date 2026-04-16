# 🚢 Docksmith

Docksmith is a simplified, from-scratch container engine and build system. It implements core containerization concepts including:

- Layered filesystems  
- Content-addressable storage  
- Deterministic build caching  
- Process isolation using Linux primitives  

---

## 🚀 Essential Setup (Required for Demo)

Docksmith works fully offline. Before building images, you must manually import the Alpine base image.

### 1. Download the Alpine RootFS
```bash
wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz
```

### 2. Initialize Docksmith Directories
```bash
mkdir -p ~/.docksmith/images ~/.docksmith/layers ~/.docksmith/cache
```

### 3. Import the Base Layer
```bash
mv alpine-minirootfs-3.20.3-x86_64.tar.gz ~/.docksmith/layers/sha256:alpine-base
```

### 4. Create the Base Image Manifest

Create the file:
```
~/.docksmith/images/alpine.json
```

Paste the following content:
```json
{
  "name": "alpine",
  "tag": "latest",
  "layers": [
    {
      "digest": "sha256:alpine-base"
    }
  ],
  "config": {
    "Env": [],
    "Cmd": ["/bin/sh"],
    "WorkingDir": "/"
  },
  "digest": "sha256:alpine-base-placeholder"
}
```

---

## 🛠 Features

### 🧱 Layered Architecture
- `COPY` and `RUN` instructions create immutable delta layers  
- Efficient storage and reuse of filesystem changes  

### ⚡ Deterministic Caching
- Each build step uses a hashing key  
- Results in:
  - `[CACHE HIT]` → reused layer  
  - `[CACHE MISS]` → new layer created  

### 🔒 Process Isolation
- Uses `chroot`  
- Prevents container processes from accessing the host filesystem  

### 📦 Metadata Management
- Image configurations stored in JSON manifests  
- Includes:
  - `ENV`
  - `WORKDIR`
  - `CMD`

---

## 💻 Usage

### 🔨 Build an Image
```bash
sudo ./docksmith build -t myapp:latest .
```

### ▶️ Run a Container
```bash
sudo ./docksmith run myapp:latest
```

### 🌱 Run with Environment Variables
```bash
sudo ./docksmith run -e GREETING=Welcome myapp:latest
```

### 📋 List Images
```bash
./docksmith images
```
