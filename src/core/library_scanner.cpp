#include "library_scanner.h"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>

namespace retrodap {

static bool hasFlacExtension(const std::string& path) {
    if (path.size() < 5) {
        return false;
    }
    std::string suffix = path.substr(path.size() - 5);
    for (size_t i = 0; i < suffix.size(); ++i) {
        suffix[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
    }
    return suffix == ".flac";
}

static void scanRecursive(const std::string& directory, std::vector<std::string>& output) {
    DIR* dir = opendir(directory.c_str());
    if (dir == NULL) {
        return;
    }

    struct dirent* entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        std::string path = directory;
        if (!path.empty() && path[path.size() - 1] != '/') {
            path += '/';
        }
        path += name;

        struct stat st;
        if (stat(path.c_str(), &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            scanRecursive(path, output);
        } else if (S_ISREG(st.st_mode) && hasFlacExtension(path)) {
            output.push_back(path);
        }
    }
    closedir(dir);
}

std::vector<std::string> LibraryScanner::scanFlacFiles(const std::string& rootPath) {
    std::vector<std::string> files;
    scanRecursive(rootPath, files);
    std::sort(files.begin(), files.end());
    return files;
}

std::vector<TrackMetadata> LibraryScanner::scanMetadata(const std::string& rootPath) {
    std::vector<std::string> files = scanFlacFiles(rootPath);
    std::vector<TrackMetadata> result;
    result.reserve(files.size());
    for (size_t i = 0; i < files.size(); ++i) {
        TrackMetadata metadata;
        if (FlacDecoder::readMetadata(files[i], metadata)) {
            std::vector<uint8_t>().swap(metadata.artwork);
            result.push_back(metadata);
        }
    }
    return result;
}

} // namespace retrodap
