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

#include <gtest/gtest.h>
#include <thread>

#include "TestActivityClient.h"

TEST(activity_client, test1) {

    ConnectionsManager manager("192.168.1.101",
                               "8080",
                               TestActorIndex::getInstance().getActorIndex()->firstId().toByteArray());

    std::ifstream dbActivity(dbActPath);

    EXPECT_EQ(true, dbActivity.good());
}


TEST(activity_client, test2) {

    ConnectionsManager manager("192.168.1.101",
                               "8080",
                               TestActorIndex::getInstance().getActorIndex()->firstId().toByteArray());

    std::array<std::string, 4> collums { "hash", "timeactivity", "activity", "score" };

    sqlite3*      db;
    sqlite3_stmt* stmt;

    const std::string sql = "PRAGMA table_info(" + ActivityTableName + ");";

    int rc = sqlite3_open(dbActPath.c_str(), &db);

    EXPECT_EQ(0, rc);

    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string columnName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        EXPECT_EQ(collums[count++], columnName);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

TEST(activity_client, test3) {

    int countConnect = 4;

    ConnectionsManager manager("192.168.1.101",
                               "8080",
                               TestActorIndex::getInstance().getActorIndex()->firstId().toByteArray());

    std::vector<Connection> client;
    for (int i = 0; i < countConnect; ++i) {
        client.push_back(Connection { std::to_string(8000 + i),
                                      "192.168.1." + std::to_string(100 + i),
                                      static_cast<bool>(i & 1) });
    }

    for (std::vector<Connection>::iterator it = client.begin(); it != client.end(); ++it) {
        manager.addActivity(*it);
    }

    std::this_thread::sleep_for(std::chrono::seconds(4));

    for (std::vector<Connection>::iterator it = client.begin() + 1; it != client.end(); ++it) {
        manager.removeActivity(*it);
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    std::unordered_map<std::string, uint64_t> score;
    for (std::vector<Connection>::iterator it = client.begin(); it != client.end(); ++it) {
        score.insert(std::pair(it->address, manager.getActivityScore(*it)));
    }

    manager.synchroActivityDB();
    manager.loadActivityRecords();

    int i = 0;
    for (std::vector<Connection>::iterator it = client.begin(); it != client.end(); ++it) {
        EXPECT_EQ(score.at(it->address), manager.getActivityScore(*it));
    }
}
