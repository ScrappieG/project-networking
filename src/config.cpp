#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>
using namespace std;


// Some helper function i creater to make things easier


const string Config::joinPath(const string& a, const string& b) const {
    filesystem::path p = a;
    p /= b;
    return p.string();
}

//spec uses "peer_<id>", so this function helps to keep the format
string Config::peerDirName(int id) {
    return "peer_" + to_string(id);
}

// load everything from the config files in config object
//catch any early fails so we dont get lost
Config Config::load(const string& root) {
    Config cfg;
    // Common.cfg
    {
        ifstream in(joinPath(root, "Common.cfg"));
        if (!in) throw runtime_error("Common.cfg not found in " + root);

        string key;
        //parses a word than its value, long but effective
        while (in >> key) {
            if (key == "NumberOfPreferredNeighbors") {
                in >> cfg.common.preferredNeighbors;
            }
            else if (key == "UnchokingInterval") {
                in >> cfg.common.unchokeIntervalSec;
            }
            else if (key == "OptimisticUnchokingInterval"){
                in >> cfg.common.optimisticUnchokeIntervalSec;
            }
            else if (key == "FileName") {
                in >> cfg.common.fileName;
            }
            else if (key == "FileSize") {
                in >> cfg.common.fileSizeBytes;
            }
            else if (key == "PieceSize") {
                in >> cfg.common.pieceSizeBytes;
            }
            else {
                string skip; getline(in, skip);
            } // ignore unknown stuff on that line
        }
    }

    // installed some early checks with messages to catch any config errors
    if (cfg.common.preferredNeighbors <= 0) {
        throw runtime_error("Common.cfg: NumberOfPreferredNeighbors must be > 0");
    }
    if (cfg.common.unchokeIntervalSec <= 0) {
        throw runtime_error("Common.cfg: UnchokingInterval must be > 0");
    }
    if (cfg.common.optimisticUnchokeIntervalSec <= 0) {
        throw runtime_error("Common.cfg: OptimisticUnchokingInterval must be > 0");
    }
    if (cfg.common.fileName.empty()) {
        throw runtime_error("Common.cfg: FileName missing/empty");
    }
    if (cfg.common.fileSizeBytes <= 0) {
        throw runtime_error("Common.cfg: FileSize must be > 0");
    }
    if (cfg.common.pieceSizeBytes <= 0) {
        throw runtime_error("Common.cfg: PieceSize must be > 0");
    }

    // Red PeerInfo.cfg
    {
        ifstream in(joinPath(root, "PeerInfo.cfg"));
        if (!in) throw runtime_error("PeerInfo.cfg not found in " + root);

        PeerRow r;
        //each line has: id , host , port , hasFile
        while (in >> r.id >> r.host >> r.port >> r.hasFile) {
            cfg.peers.push_back(r);
        }
    }
    if (cfg.peers.empty()) {
        throw runtime_error("PeerInfo.cfg: no peers listed");
    }

    //heads-up if last piece won’t be full
    if (cfg.common.fileSizeBytes % cfg.common.pieceSizeBytes != 0) {
        cerr << "[warn] FileSize isn’t a multiple of PieceSize — last piece will be partial.\n";
    }

    return cfg;
}

//finds a peer by their row id
PeerRow * Config::getPeer(int id) const {
    for (auto& p : peers) {
        if (p.id == id) {
            return const_cast<PeerRow *>(&p);
        }
    }
    return nullptr;

}

// quick file/dir helpers using <filesystem>
bool isFile(const string& path) {
    error_code ec;
    return filesystem::is_regular_file(path, ec);
}
bool isDir(const string& path) {
    error_code ec;
    return filesystem::is_directory(path, ec);
}
long long sizeOfFile(const string& path) {
    error_code ec;
    auto sz = filesystem::file_size(path, ec);
    if (ec) return -1;
    return static_cast<long long>(sz);
}

//makes sure the folders and/or files for this peer actually exist and match the config
void Config::validateFilesystem(const string& root, int selfId) const {
    auto me = getPeer(selfId);
    if (!me) throw runtime_error("Self peer ID " + to_string(selfId) + " not found in PeerInfo.cfg");

    const string myDir = joinPath(root, peerDirName(selfId));
    if (!isDir(myDir))
        throw runtime_error("Missing folder: " + myDir + " (create it next to Common.cfg)");

    if (me->hasFile) {
        const string seedPath = joinPath(myDir, common.fileName);
        if (!isFile(seedPath)) throw runtime_error("Seed file missing: " + seedPath);
        const long long sz = sizeOfFile(seedPath);
        if (sz != common.fileSizeBytes) {
            throw runtime_error("Seed file size mismatch: " + to_string(sz) +
                                " vs FileSize " + to_string(common.fileSizeBytes));
        }
    }
}
