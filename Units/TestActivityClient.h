/*
 * ExtraChain Console Client
 * Copyright (C) 2025 ExtraChain Foundation <official@extrachain.io>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

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

    TestActorIndex(const TestActorIndex&)            = delete;
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
