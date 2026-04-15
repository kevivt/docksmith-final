#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <sstream>
#include <unistd.h>
#include <stdlib.h>
#include <algorithm>
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// --- UTILITIES ---

std::string getDocksmithDir() {
    const char* sudoUser = getenv("SUDO_USER");
    std::string homeDir = (sudoUser != nullptr) ? "/home/" + std::string(sudoUser) : (getenv("HOME") ? std::string(getenv("HOME")) : ".");
    return homeDir + "/.docksmith";
}

int systemExec(const std::string& cmd) { return system(cmd.c_str()); }

std::string sha256(const std::string& input) {
    std::string dsDir = getDocksmithDir();
    std::string tempFile = dsDir + "/.temp_hash";
    std::ofstream out(tempFile); out << input; out.close();
    std::string cmd = "sha256sum " + tempFile + " | awk '{print $1}'";
    FILE* pipe = popen(cmd.c_str(), "r");
    char buffer[128]; std::string result = "";
    while (fgets(buffer, 128, pipe) != NULL) result += buffer;
    pclose(pipe); fs::remove(tempFile);
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

std::string sha256File(const std::string& filepath) {
    std::string cmd = "sha256sum " + filepath + " | awk '{print $1}'";
    FILE* pipe = popen(cmd.c_str(), "r");
    char buffer[128]; std::string result = "";
    while (fgets(buffer, 128, pipe) != NULL) result += buffer;
    pclose(pipe);
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

std::string getDirHash(const std::string& path) {
    if (!fs::exists(path)) return "";
    if (!fs::is_directory(path)) return sha256File(path);
    std::string cmd = "find " + path + " -type f -not -path '*/.*' -exec sha256sum {} + | sort | sha256sum | awk '{print $1}'";
    FILE* pipe = popen(cmd.c_str(), "r");
    char buffer[128]; std::string result = "";
    while (fgets(buffer, 128, pipe) != NULL) result += buffer;
    pclose(pipe);
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

void printHelp() {
    std::cout << "Docksmith usage:\n  build -t <tag> <context>\n  run [-e KEY=VAL] <tag>\n  images\n  rmi <tag>\n";
}

// --- MAIN ENGINE ---

int main(int argc, char* argv[]) {
    if (argc < 2) { printHelp(); return 1; }
    std::string command = argv[1];
    std::string dsDir = getDocksmithDir();
    fs::create_directories(dsDir + "/images");
    fs::create_directories(dsDir + "/layers");
    fs::create_directories(dsDir + "/cache");

    if (command == "build") {
        if (argc < 5) { printHelp(); return 1; }
        std::string tag = argv[3];
        std::string context = argv[4];
        std::string dfPath = context + "/Docksmithfile";
        if (!fs::exists(dfPath)) { std::cerr << "No Docksmithfile found!\n"; return 1; }
        
        std::ifstream f(dfPath); std::string line;
        std::vector<std::string> instructions;
        while (std::getline(f, line)) {
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            if (!line.empty() && line[0] != '#') instructions.push_back(line);
        }

        json buildLayers = json::array();
        json buildConfig = {{"Env", json::array()}, {"Cmd", json::array()}, {"WorkingDir", "/"}};
        std::string baseDigest = "";
        std::vector<std::string> currentEnv;

        auto buildStart = std::chrono::high_resolution_clock::now();
        int step = 1;
        int totalSteps = instructions.size();

        for (const auto& inst : instructions) {
            auto stepStart = std::chrono::high_resolution_clock::now();
            std::cout << "Step " << step << "/" << totalSteps << ": " << inst << " ";
            
            if (inst.rfind("FROM ", 0) == 0) {
                std::string base = inst.substr(5);
                std::string imgName = base.find(':') != std::string::npos ? base.substr(0, base.find(':')) : base;
                std::ifstream imgF(dsDir + "/images/" + imgName + ".json");
                if (!imgF) { std::cout << "\nError: Base image " << base << " not found in local store!\n"; return 1; }
                json manifest; imgF >> manifest;
                buildLayers = manifest["layers"];
                buildConfig = manifest["config"];
                for (auto& e : buildConfig["Env"]) currentEnv.push_back(e.get<std::string>());
                baseDigest = manifest.value("digest", "");
                std::cout << "\n";
            } 
            else if (inst.rfind("COPY ", 0) == 0 || inst.rfind("RUN ", 0) == 0) {
                std::string parent = buildLayers.empty() ? baseDigest : buildLayers.back()["digest"].get<std::string>();
                std::string envHash = "";
                std::vector<std::string> sortedEnv = currentEnv;
                std::sort(sortedEnv.begin(), sortedEnv.end());
                for (const auto& e : sortedEnv) envHash += e + "|";
                
                std::string cacheStr = parent + inst + buildConfig["WorkingDir"].get<std::string>() + envHash;
                if (inst.rfind("COPY ", 0) == 0) cacheStr += getDirHash(context);
                
                std::string key = sha256(cacheStr);
                bool cacheHit = false;

                if (fs::exists(dsDir + "/cache/" + key)) {
                    std::ifstream cf(dsDir + "/cache/" + key);
                    std::string d; cf >> d;
                    if (fs::exists(dsDir + "/layers/" + d)) {
                        buildLayers.push_back({{"digest", d}});
                        cacheHit = true;
                        auto stepEnd = std::chrono::high_resolution_clock::now();
                        std::chrono::duration<double> diff = stepEnd - stepStart;
                        std::cout << "[CACHE HIT] " << std::fixed << std::setprecision(2) << diff.count() << "s\n";
                    }
                }

                if (!cacheHit) {
                    std::string layerDigest;
                    if (inst.rfind("COPY ", 0) == 0) {
                        std::string args = inst.substr(5);
                        size_t space = args.find(' ');
                        std::string src = args.substr(0, space);
                        std::string dest = args.substr(space + 1);
                        std::string tmp = dsDir + "/.tmp_layer";
                        fs::remove_all(tmp); fs::create_directories(tmp + (dest[0] == '/' ? "" : "/") + dest);
                        systemExec("cp -r " + context + "/" + src + " " + tmp + (dest[0] == '/' ? "" : "/") + dest);
                        std::string tar = dsDir + "/layer.tar";
                        systemExec("cd " + tmp + " && tar --mtime='1970-01-01' -cf " + tar + " .");
                        layerDigest = "sha256:" + sha256File(tar);
                        fs::rename(tar, dsDir + "/layers/" + layerDigest);
                    } else {
                        std::string runCmd = inst.substr(4);
                        std::string root = dsDir + "/.root";
                        fs::remove_all(root); fs::create_directories(root);
                        for (auto& l : buildLayers) {
                            std::string d = l["digest"].get<std::string>();
                            systemExec("tar --warning=no-timestamp -xf " + dsDir + "/layers/" + d + " -C " + root);
                        }
                        systemExec("touch " + root + "/.time");
                        std::string workDir = buildConfig["WorkingDir"].get<std::string>();
                        
                        std::string envExports = "";
                        for (const auto& e : currentEnv) envExports += "export " + e + " && ";
                        
                        std::string chrootCmd = "chroot " + root + " /bin/sh -c '" + envExports + "mkdir -p " + workDir + " && cd " + workDir + " && " + runCmd + "'";
                        systemExec(chrootCmd);
                        std::string tar = dsDir + "/layer.tar";
                        systemExec("cd " + root + " && find . -newer .time -not -name .time > .list && if [ -s .list ]; then tar --no-recursion --mtime='1970-01-01' -cf " + tar + " -T .list; else tar --mtime='1970-01-01' -cf " + tar + " -T /dev/null; fi");
                        layerDigest = "sha256:" + sha256File(tar);
                        fs::rename(tar, dsDir + "/layers/" + layerDigest);
                    }
                    buildLayers.push_back({{"digest", layerDigest}});
                    std::ofstream cOut(dsDir + "/cache/" + key); cOut << layerDigest;
                    
                    auto stepEnd = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> diff = stepEnd - stepStart;
                    std::cout << "[CACHE MISS] " << std::fixed << std::setprecision(2) << diff.count() << "s\n";
                }
            }
            else if (inst.rfind("WORKDIR ", 0) == 0) { buildConfig["WorkingDir"] = inst.substr(8); std::cout << "\n"; }
            else if (inst.rfind("ENV ", 0) == 0) { 
                std::string kv = inst.substr(4);
                buildConfig["Env"].push_back(kv); 
                currentEnv.push_back(kv);
                std::cout << "\n"; 
            }
            else if (inst.rfind("CMD ", 0) == 0) { buildConfig["Cmd"] = json::parse(inst.substr(4)); std::cout << "\n"; }
            step++;
        }

        std::string name = tag.substr(0, tag.find(':'));
        json manifest = {{"name", name}, {"tag", "latest"}, {"layers", buildLayers}, {"config", buildConfig}};
        
        // Canonical digest generation
        manifest["digest"] = "";
        std::string finalDigest = "sha256:" + sha256(manifest.dump());
        manifest["digest"] = finalDigest;
        
        std::ofstream(dsDir + "/images/" + name + ".json") << manifest.dump(4);
        auto buildEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> totalDiff = buildEnd - buildStart;
        std::cout << "Successfully built " << finalDigest << " " << tag << " (" << std::fixed << std::setprecision(2) << totalDiff.count() << "s)\n";
    }
    else if (command == "images") {
        std::cout << std::left << std::setw(20) << "NAME" << std::setw(15) << "TAG" << "ID\n";
        for (const auto& entry : fs::directory_iterator(dsDir + "/images")) {
            std::ifstream f(entry.path()); json m; f >> m;
            std::string d = m.value("digest", "unknown");
            std::string shortId = d.length() > 19 ? d.substr(7, 12) : d;
            std::cout << std::left << std::setw(20) << m["name"].get<std::string>() 
                      << std::setw(15) << m["tag"].get<std::string>() << shortId << "\n";
        }
    }
    else if (command == "rmi") {
        if (argc < 3) { std::cerr << "Missing image tag\n"; return 1; }
        std::string tag = argv[2];
        std::string name = tag.substr(0, tag.find(':'));
        std::string imgPath = dsDir + "/images/" + name + ".json";
        if (!fs::exists(imgPath)) { std::cerr << "Error: Image " << tag << " not found\n"; return 1; }
        
        std::ifstream f(imgPath); json m; f >> m;
        for (auto& l : m["layers"]) {
            std::string d = l["digest"].get<std::string>();
            fs::remove(dsDir + "/layers/" + d);
        }
        fs::remove(imgPath);
        std::cout << "Untagged and removed " << tag << " and its layers.\n";
    }
    else if (command == "run") {
        std::vector<std::string> overrides;
        int imgIndex = 2;
        while (imgIndex < argc && std::string(argv[imgIndex]) == "-e") {
            if (imgIndex + 1 < argc) { overrides.push_back(argv[imgIndex + 1]); imgIndex += 2; }
            else { std::cerr << "Missing value for -e\n"; return 1; }
        }
        if (imgIndex >= argc) { std::cerr << "Missing image tag\n"; return 1; }
        
        std::string tag = argv[imgIndex];
        std::string name = tag.substr(0, tag.find(':'));
        std::ifstream f(dsDir + "/images/" + name + ".json");
        if (!f) { std::cerr << "Image not found\n"; return 1; }
        
        json m; f >> m;
        std::string root = dsDir + "/.run_root";
        fs::remove_all(root); fs::create_directories(root);
        
        for (auto& l : m["layers"]) {
            std::string d = l["digest"].get<std::string>();
            systemExec("tar --warning=no-timestamp -xf " + dsDir + "/layers/" + d + " -C " + root);
        }
        
        std::string envExports = "";
        for (auto& e : m["config"]["Env"]) envExports += "export " + e.get<std::string>() + " && ";
        for (auto& e : overrides) envExports += "export " + e + " && ";
        
        std::string execCmd = m["config"]["Cmd"].empty() ? "/bin/sh" : m["config"]["Cmd"][0].get<std::string>();
        std::string workDir = m["config"].value("WorkingDir", "/");
        std::string chrootCmd = "chroot " + root + " /bin/sh -c '" + envExports + "cd " + workDir + " && " + execCmd + "'";
        int status = systemExec(chrootCmd);
        std::cout << "\nContainer exited with code " << WEXITSTATUS(status) << "\n";
    }
    return 0;
}
