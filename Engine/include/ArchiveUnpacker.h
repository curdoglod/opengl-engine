#pragma once

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <filesystem>

class ArchiveUnpacker {
public:
    ArchiveUnpacker(const std::string& folderPath)
        : folderPath(folderPath) {
    }

    bool Unpack() {
        namespace fs = std::filesystem;
        fs::path path(folderPath);
        if (!fs::exists(path) || !fs::is_directory(path)) {
            std::cerr << "Invalid folder: " << folderPath << std::endl;
            return false;
        }

        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (fs::is_regular_file(entry.path())) {
                if (!UnpackCurrentFile(entry.path())) {
                    std::cerr << "Failed to read file: " << entry.path() << std::endl;
                }
            }
        }

        return true;
    }

    std::vector<unsigned char> GetFile(const std::string& filePath) const {
        auto it = unpackedFiles.find(filePath);
        if (it != unpackedFiles.end()) {
            return it->second;
        }
        else {
            std::cerr << "File " << filePath << " not found." << std::endl;
            return {};
        }
    }

private:
    std::string folderPath;
    std::map<std::string, std::vector<unsigned char>> unpackedFiles;

    bool UnpackCurrentFile(const std::filesystem::path& filePath) {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file) {
            std::cerr << "Unable to open file: " << filePath << std::endl;
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<unsigned char> buffer(size);

        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            std::cerr << "Error reading file: " << filePath << std::endl;
            return false;
        }

        std::filesystem::path relativePath = std::filesystem::relative(filePath, folderPath);
        unpackedFiles[relativePath.string()] = std::move(buffer);
        return true;
    }
};
