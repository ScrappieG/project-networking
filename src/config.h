
#pragma once
#include <string>
#include <vector>

using namespace std;

//cfg structre
struct CommonCfg{
    int preferredNeighbors = 3;
    int unchokeIntervalSec = 5;
    int optimisticUnchokeIntervalSec = 10;
    string fileName = "tree.png";
    long long fileSizeBytes = 24301474;
    int pieceSizeBytes = 16384;

    int pieceCount() const {
        if (pieceSizeBytes <= 0) return 0;
        return static_cast<int>((fileSizeBytes + pieceSizeBytes - 1) / pieceSizeBytes);
    }
};
struct PeerRow {
    int id= -1;
    string host;
    int port= 0;
    bool hasFile =false;
};

struct Config {
    CommonCfg common;
    vector<PeerRow> peers;

    // Load both cfg files from root 
    Config load(const string& root = ".");

    // helpers
    const string joinPath(const string& a, const string& b) const;
    static string peerDirName(int id); // "peer_1001"

    void validateFilesystem(const string& root, int selfId) const;

    // lookups
    PeerRow * getPeer(int id) const;
};







