#ifndef TESTACTIVITYCLIENT_H
#define TESTACTIVITYCLIENT_H

#include "managers/connections_manager.h"
#include "datastorage/index/actorindex.h"
#include <sqlite3.h>
#include <filesystem>

namespace fs = std::filesystem;

class TestActorIndex {

    ActorIndex* actorIndex;

    TestActorIndex() {
        ExtraChainNode node;
        actorIndex = new ActorIndex(node);
    }

    ~TestActorIndex() {
        delete actorIndex;
    }

    TestActorIndex(const TestActorIndex&) = delete;
    TestActorIndex& operator=(const TestActorIndex&) = delete;


public:
    static TestActorIndex& getInstance() {
        std::filesystem::path filePath = dbActPath;
        if (fs::exists(filePath))
            fs::remove(filePath);
        static TestActorIndex instance;
        return instance;
    }


    ActorIndex* getActorIndex() const {
        return actorIndex;
    }
};


#endif // TESTACTIVITYCLIENT_H
