#pragma once

#include "data/tileSource.h"
#include <string>
#include <memory>
#include <vector>
#include <mutex>

namespace pmtiles {
    struct headerv3;
}

namespace Tangram {

class Platform;
class AsyncWorker;

// PMTilesDataSource provides tile data from PMTiles archives
// Supports both local files and HTTP sources with range requests
class PMTilesDataSource : public TileSource::DataSource {
public:
    // Constructor for local file or HTTP source
    PMTilesDataSource(Platform& _platform, const std::string& _path);
    
    ~PMTilesDataSource() override;

    bool loadTileData(std::shared_ptr<TileTask> _task, TileTaskCb _cb) override;

    void clear() override;

private:
    // Read a range of bytes from the PMTiles file
    // Returns true on success, false on failure
    bool readRange(uint64_t offset, uint32_t length, std::vector<char>& data);
    
    // Get tile data for a specific tile ID
    bool getTileData(const TileID& _tileId, std::vector<char>& _data);
    
    // Decompress data based on compression type
    bool decompress(const std::vector<char>& compressed, std::vector<char>& decompressed, uint8_t compression);
    
    // Load and cache the header
    bool loadHeader();
    
    // Load and cache directory data for faster lookups
    bool loadDirectory(uint64_t offset, uint32_t length, std::vector<char>& data);

    Platform& m_platform;
    std::string m_path;
    bool m_isHttp;
    
    // Cached header
    std::unique_ptr<pmtiles::headerv3> m_header;
    
    // Worker for async operations
    std::unique_ptr<AsyncWorker> m_worker;
    
    // Mutex for thread safety
    std::mutex m_mutex;
    
    // Cache for root directory (speeds up tile lookups)
    std::vector<char> m_rootDirCache;
    bool m_rootDirCached;
};

}
