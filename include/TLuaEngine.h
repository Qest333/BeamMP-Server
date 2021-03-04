#pragma once

#include "Common.h"
#include "IThreaded.h"
#include "TLuaFile.h"
#include "TServer.h"
#include <optional>
#include <lua.hpp>
#include <memory>
#include <set>

class TLuaEngine : public IThreaded {
public:
    explicit TLuaEngine(TServer& Server, TTCPServer& TCPServer, TUDPServer& UDPServer);
    //~TLuaEngine();

    using TSetOfLuaFile = std::set<std::unique_ptr<TLuaFile>>;

    void operator()() override;

    [[nodiscard]] const TSetOfLuaFile& LuaFiles() const { return mLuaFiles; }
    [[nodiscard]] TServer& Server() { return mServer; }
    [[nodiscard]] const TServer& Server() const { return mServer; }

    std::optional<std::reference_wrapper<TLuaFile>> GetScript(lua_State* L);

    TTCPServer& TCPServer() { return mTCPServer; }
    TUDPServer& UDPServer() { return mUDPServer; }

private:
    void FolderList(const std::string& Path, bool HotSwap);
    void RegisterFiles(const std::string& Path, bool HotSwap);
    bool NewFile(const std::string& Path);

    TTCPServer& mTCPServer;
    TUDPServer& mUDPServer;
    TServer& mServer;
    std::string mPath;
    bool mShutdown { false };
    TSetOfLuaFile mLuaFiles;
};