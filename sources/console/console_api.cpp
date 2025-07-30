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

#include "console/console_api.h"

#include "crow.h"
#include <chrono>
#include <regex>

#include "managers/extrachain_node.h"
#include "chain/dag.h"

long long parseTimeToMs(const std::string& time_str) {
    if (time_str.empty())
        return 24 * 60 * 60 * 1000; // 1 day

    std::regex  pattern(R"((?:(\d+)d)?(?:(\d+)h)?(?:(\d+)m)?(?:(\d+)s)?)");
    std::smatch matches;

    if (!std::regex_match(time_str, matches, pattern)) {
        return 24 * 60 * 60 * 1000; // 1day
    }

    long long totalMs = 0;
    if (matches[1].matched)
        totalMs += std::stoll(matches[1]) * 24 * 60 * 60 * 1000; // day
    if (matches[2].matched)
        totalMs += std::stoll(matches[2]) * 60 * 60 * 1000; // hours
    if (matches[3].matched)
        totalMs += std::stoll(matches[3]) * 60 * 1000; // minutes
    if (matches[4].matched)
        totalMs += std::stoll(matches[4]) * 1000; // seconds

    return totalMs > 0 ? totalMs : 24 * 60 * 60 * 1000;
}

void run_api(ExtraChainNode* node) {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/balance").methods("POST"_method)([&node](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("actor_id")) {
            return crow::response(400, R"({"error": "actor_id required"})");
        }

        std::string actorIdStr = json["actor_id"].s();
        auto        actor_id   = ActorId::create(actorIdStr);
        if (!actor_id.has_value()) {
            return crow::response(400, R"({"error": "invalid actor_id"})");
        }
        TokenId tokenId("468faf2f1be6504a9a26f7f027f7e43380b0d77d");

        if (!node->actorIndex()->exists(actor_id.value())) {
            return crow::response(404, R"({"error": "actor not found"})");
        }

        std::map<std::pair<ActorId, TokenId>, BigNumberFloat> balances =
            node->dag()->calculate_actors_balance({ actor_id.value() });

        BigNumberFloat balance    = BigNumberFloat(0); // default
        auto           balanceKey = std::make_pair(actor_id.value(), tokenId);
        auto           it         = balances.find(balanceKey);
        if (it != balances.end()) {
            balance = it->second;
        }

        crow::json::wvalue response;
        response["actor_id"] = actor_id.value().to_string();
        response["balance"]  = balance.to_string(NumeralBase::Dec);

        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/have_rewards").methods("POST"_method)([&node](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("actor_id")) {
            return crow::response(400, R"({"error": "actor_id required"})");
        }

        std::string actorId  = json["actor_id"].s();
        std::string period   = json.has("period") ? json["period"].s() : std::string();
        long long   periodMs = parseTimeToMs(period);

        auto actor_id = ActorId::create(actorId);
        if (!actor_id.has_value()) {
            return crow::response(400, R"({"error": "invalid actor_id"})");
        }

        // Вычисляем время отсечки
        auto now        = std::chrono::system_clock::now().time_since_epoch();
        auto cutoffTime = std::chrono::milliseconds(now.count() - periodMs);

        bool hasRewards  = false;
        int  rewardCount = 0;

        const std::vector<BigNumber> sections = node->dag()->cache().read_index(actor_id.value());
        for (const auto& section_id : sections) {
            auto section = node->dag()->read_section(section_id);
            if (!section.has_value()) {
                continue;
            }

            for (const auto& tx : section.value().transactions) {
                if (tx.type() == TransactionType::Reward) {
                    // Получаем время транзакции (нужно реализовать tx.timestamp() или подобное)
                    auto txTime       = tx.timestamp(); // предполагаем что возвращает std::chrono::milliseconds
                    auto cutoffTimeMs = static_cast<std::uint64_t>(now.count() - periodMs);

                    if (txTime >= cutoffTimeMs) {
                        hasRewards = true;
                        rewardCount++;
                    } else {
                        // Если транзакция старше периода, прерываем (если секции отсортированы по времени)
                        break;
                    }
                }
            }

            // Если нужно выйти за пределы времени на уровне секций
            // auto sectionTime = section.value().timestamp();
            // if (sectionTime < cutoffTime) break;
        }

        crow::json::wvalue response;
        response["actor_id"]     = actor_id.value().to_string();
        response["has_rewards"]  = hasRewards;
        response["reward_count"] = rewardCount;
        response["period_ms"]    = periodMs;
        response["period_str"]   = period.empty() ? "1d" : period;

        return crow::response(200, response);
    });

    std::uint16_t port = 8080;
    app.port(port).concurrency(2).run();
    eLog("Started api on port {}", port);
}
