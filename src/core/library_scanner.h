#ifndef RETRODAP_LIBRARY_SCANNER_H
#define RETRODAP_LIBRARY_SCANNER_H

#include <string>
#include <vector>

#include "flac_decoder.h"

namespace retrodap {

class LibraryScanner {
public:
    static std::vector<std::string> scanFlacFiles(const std::string& rootPath);
    static std::vector<TrackMetadata> scanMetadata(const std::string& rootPath);
};

} // namespace retrodap

#endif
