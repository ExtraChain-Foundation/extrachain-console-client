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
#include "dfs/dfs_controller.h"

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
    std::string     token_session = "26981bbf8819c458b971861591fdc5e3ecc0876e0e3742d8634d79680fc8e89c";
    eLog("API runned.");

    auto contains = [&](std::vector<std::string> fields, const std::string& value) {
        return std::find(fields.begin(), fields.end(), value) != fields.end();
    };

    CROW_ROUTE(app, "/balance").methods("POST"_method)([&node, &token_session](const crow::request& req) {
        auto json = crow::json::load(req.body);

        if (!json || !json.has("actor_id")) {
            return crow::response(400, R"({"error": "actor_id required"})");
        }

        std::string token = std::string(json["token"].s());
        if (!json.has("token") || token.empty()) {
            return crow::response(400, R"({"error": "missing or empty token."})");
        }

        if (token != token_session) {
            return crow::response(400, R"({"error": "token is not valid."})");
        }

        std::string actorIdStr = json["actor_id"].s();
        auto        actor_id   = ActorId::create(actorIdStr);
        if (!actor_id.has_value()) {
            return crow::response(400, R"({"error": "invalid actor_id"})");
        }
        TokenId tokenId("468faf2f1be6504a9a26f7f027f7e43380b0d77d");

        if (!node->actor_index()->exists(actor_id.value())) {
            return crow::response(404, R"({"error": "actor not found"})");
        }

        std::map<std::pair<ActorId, TokenId>, BigNumberFloat> balances =
            node->dag()->calculate_actors_balance({ actor_id.value() });

        eLog("[api] [POST] [transaction_by_hash_and_section_id] [actor_id: {}]", actorIdStr);

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

    // section_id and hash
    CROW_ROUTE(app, "/transaction_by_hash_and_section_id")
        .methods("POST"_method)([&node, &token_session](const crow::request& req) {
            auto json = crow::json::load(req.body);
            if (!json || !json.has("hash")) {
                return crow::response(400, R"({"error": "hash required"})");
            }

            if (!json.has("section_id")) {
                return crow::response(400, R"({"error": "section_id required"})");
            }

            if (!json.has("token")) {
                return crow::response(400, R"({"error": "token required"})");
            }

            std::string token = std::string(json["token"].s());
            if (token.empty()) {
                return crow::response(400, R"({"error": "token empty."})");
            }

            if (token != token_session) {
                return crow::response(400, R"({"error": "token is not valid."})");
            }

            std::string hash          = json["hash"].s();
            int         sectionNumber = json["section_id"].i();
            auto        section       = node->dag()->read_section(SectionId(sectionNumber));
            auto        transactions  = section->transactions;
            if (transactions.empty()) {
                return crow::response(400, fmt::format(R"({{"error": "list transactions is empty"}})"));
            }

            eLog("[api] [POST] [transaction_by_hash_and_section_id] [hash: {}]", hash);

            auto it = std::find_if(transactions.begin(), transactions.end(), [&hash](const Transaction& t) {
                return t.hash() == hash;
            });

            if (it != transactions.end()) {
                Transaction        tx = *it;
                crow::json::wvalue response;
                response["hash"]     = hash;
                response["sender"]   = tx.sender().to_string();
                response["receiver"] = tx.receiver().to_string();
                response["amount"]   = tx.amount().to_string(NumeralBase::Dec);
                auto dateTime        = QDateTime::fromMSecsSinceEpoch(tx.timestamp());
                response["date"]     = dateTime.toString("dd/MM/yyyy").toStdString();
                response["time"]     = dateTime.toString("hh:mm:ss").toStdString();
                std::string typeTx;
                switch (tx.type()) {
                case TransactionType::Genesis:
                    typeTx = "genesis";
                    break;
                case TransactionType::Balance:
                    typeTx = "balance";
                    break;
                case TransactionType::Burn:
                    typeTx = "burn";
                    break;
                case TransactionType::InitContract:
                    typeTx = "init_contract";
                    break;
                case TransactionType::Conversion:
                    typeTx = "conversion";
                    break;
                case TransactionType::Regular:
                    typeTx = "regular";
                    break;
                case TransactionType::Repeatable:
                    typeTx = "repeatable";
                    break;
                case TransactionType::Reward:
                    typeTx = "reward";
                    break;
                default:
                    break;
                }
                response["type"] = typeTx;

                return crow::response(200, response);
            }

            return crow::response(400, fmt::format(R"({{"error": "can not found transaction."}})"));
        });

    CROW_ROUTE(app, "/count_sections")
        .methods("GET"_method)([&node, &token_session, &contains](const crow::request& req) {
            auto keys = req.url_params.keys();
            if (!contains(keys, "token")) {
                return crow::response(400, R"({"error": "token required"})");
            }

            auto        token_param = req.url_params.get("token");
            std::string token       = std::string(token_param);
            if (token.empty()) {
                return crow::response(400, R"({"error": "token is empty."})");
            }

            if (token != token_session) {
                return crow::response(400, R"({"error": "token is not valid."})");
            }
            eLog("[api] [GET] [count_sections]");
            crow::json::wvalue response;
            response["count_sections"] = node->dag()->current_section().to_string(NumeralBase::Dec);
            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/count_transactions_in_section")
        .methods("GET"_method)([&node, &token_session, &contains](const crow::request& req) {
            auto keys = req.url_params.keys();

            if (!contains(keys, "number_section") || !contains(keys, "token")) {
                return crow::response(400, R"({"error": "number_section and token required"})");
            }

            auto number_section = req.url_params.get("number_section");
            auto token_param    = req.url_params.get("token");
            if (!number_section) {
                return crow::response(400, R"({"error": "number_section required"})");
            }

            int  section_number = std::stoi(number_section);
            auto section        = node->dag()->read_section(BigNumber(section_number));

            if (!section.has_value()) {
                return crow::response(400, R"({"error": "invalid number section"})");
            }

            std::string token = std::string(token_param);
            if (token.empty()) {
                return crow::response(400, R"({"error": "token is empty."})");
            }

            if (token != token_session) {
                return crow::response(400, R"({"error": "token is not valid."})");
            }

            eLog("[api] [GET] [count_transactions_in_section] [number_section: {}]", number_section);
            auto countTx = section->transactions.size();

            crow::json::wvalue response;
            response["count_transactions"] = countTx;
            response["section_number"]     = number_section;

            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/have_rewards").methods("POST"_method)([&node, &token_session](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("actor_id")) {
            return crow::response(400, R"({"error": "actor_id required"})");
        }

        std::string token = std::string(json["token"].s());
        if (!json.has("token") || token.empty()) {
            return crow::response(400, R"({"error": "missing or empty token."})");
        }

        if (token != token_session) {
            return crow::response(400, R"({"error": "token is not valid."})");
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
                    auto txTime = tx.timestamp(); // предполагаем что возвращает std::chrono::milliseconds
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

    CROW_ROUTE(app, "/subscription_state")
        .methods("POST"_method)([&node, &token_session](const crow::request& req) {
            auto json = crow::json::load(req.body);
            if (!json || !json.has("actor_id")) {
                return crow::response(400, R"({"error": "actor_id required"})");
            }

            std::string actorId = json["actor_id"].s();

            auto actor_id = ActorId::create(actorId);
            if (!actor_id.has_value()) {
                return crow::response(400, R"({"error": "invalid actor_id"})");
            }

            std::string token = std::string(json["token"].s());
            if (!json.has("token") || token.empty()) {
                return crow::response(400, R"({"error": "missing or empty token."})");
            }

            if (token != token_session) {
                return crow::response(400, R"({"error": "token is not valid."})");
            }

            auto raccoon_id = ActorId("46710a2d823c23db9fc2ac01e0f84212a8128373");

            auto search_result =
                Dfs::Tables::DirsFile::ActorSpace::search_file_by_folder_and_name(node->dfs()->get_db_instance(), raccoon_id,
                                                                          Dfs::Basic::TEMPLATE_VECTOR,
                                                                          "RaccoonSubscription");
            if (!search_result.has_value()) {
                return crow::response(400, R"({"error": "can not find subscription"})");
            }

            std::string sub_file_id     = search_result->file_id;
            bool        subscribeActive = false;
            bool        subscribed      = false;

            if (search_result->state == Dfs::FileState::Ready) {
                subscribeActive = true;
            }

            eLog("[api] [GET] [subscription_state] [actor_id: {}]", actorId);
            auto row = node->dfs()->read_vector_row(raccoon_id, sub_file_id, actor_id->to_string());

            if (subscribed != row.has_value()) {
                subscribed = row.has_value();
            }

            crow::json::wvalue response;
            response["actor_id"]   = actor_id.value().to_string();
            response["active"]     = subscribeActive;
            response["subscribed"] = subscribed;

            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/get_actor")
        .methods("GET"_method)([&node, &token_session, &contains](const crow::request& req) {
            auto keys = req.url_params.keys();

            auto id = req.url_params.get("id");
            auto token_param    = std::string(req.url_params.get("token"));
            if (!id) {
                return crow::response(400, R"({"error": "id required"})");
            }
            if (token_param.empty()) {
                return crow::response(400, R"({"error": "token is empty."})");
            }
            if (token_param != token_session) {
                return crow::response(400, R"({"error": "token is not valid."})");
            }


            auto res = node->actor_index()->read_actor(ActorId(id));
            if (res.has_value())
            {
                crow::json::wvalue response;
                response["public_key"]     = Utils::to_base64(res.value().key().public_key());

                return crow::response(200, response);
            }

            return crow::response(400, R"({"error": "No actor"})");
        });

    CROW_ROUTE(app, "/verify_actor")
        .methods("GET"_method)([&node, &token_session, &contains](const crow::request& req) {
            auto keys = req.url_params.keys();

            auto id = req.url_params.get("id");
            auto sig_str = req.url_params.get("signature");
            auto token_param    = std::string(req.url_params.get("token"));
            if (!id) {
                return crow::response(400, R"({"error": "id required"})");
            }
            if (!sig_str) {
                return crow::response(400, R"({"error": "signature required"})");
            }
            if (token_param.empty()) {
                return crow::response(400, R"({"error": "token is empty."})");
            }
            if (token_param != token_session) {
                return crow::response(400, R"({"error": "token is not valid."})");
            }


            auto actor_data =node->actor_index()->read_actor(ActorId(id));
            if (actor_data.has_value())
            {
                auto decoded_sig =  Utils::from_base64<std::vector<std::uint8_t>>(sig_str);
                if (!decoded_sig)
                    return crow::response(400, R"({"error": "Base64 decoding failed."})");

                const auto& decoded = decoded_sig.value();
                if (decoded.size() != crypto_sign_BYTES)
                    return crow::response(400, R"({"error": "Invalid signature length."})");

                Signature signature;
                std::copy(decoded.begin(), decoded.end(), signature.begin());

                std::string data;
                auto res = actor_data->key().verify(data, signature);

                if (res.has_value())
                {
                    crow::json::wvalue response;
                    response["result"]     = res.value();

                    return crow::response(200, response);
                }
            }

            return crow::response(400, R"({"error": "No data"})");
        });

    CROW_ROUTE(app, "/get_devices")
        .methods("GET"_method)([&node, &token_session, &contains](const crow::request& req) {
            auto keys = req.url_params.keys();

            auto token_param    = std::string(req.url_params.get("token"));
            if (token_param.empty()) {
                return crow::response(400, R"({"error": "token is empty."})");
            }
            if (token_param != token_session) {
                return crow::response(400, R"({"error": "token is not valid."})");
            }


            if (true)
            {
                std::string jsonTemp = R"([
                    {
                        "gpu_model": "NVIDIA GeForce RTX 4090",
                        "gpu_count": 1,
                        "cpu_model": "AMD Ryzen 9 7950X",
                        "cpu_cores": 16,
                        "ram": 32768,
                        "vram": 24576,
                        "ssd": true
                    },
                    {
                        "gpu_model": "NVIDIA A100",
                        "gpu_count": 4,
                        "cpu_model": "Intel Xeon Platinum 8380",
                        "cpu_cores": 40,
                        "ram": 262144,
                        "vram": 40960,
                        "ssd": true
                    },
                    {
                        "gpu_model": "AMD Radeon RX 7900 XTX",
                        "gpu_count": 2,
                        "cpu_model": "AMD Ryzen Threadripper PRO 5995WX",
                        "cpu_cores": 64,
                        "ram": 131072,
                        "vram": 24576,
                        "ssd": true
                    },
                    {
                        "gpu_model": "NVIDIA RTX 6000 Ada",
                        "gpu_count": 1,
                        "cpu_model": "Intel Core i9-14900K",
                        "cpu_cores": 24,
                        "ram": 65536,
                        "vram": 49152,
                        "ssd": true
                    },
                    {
                        "gpu_model": "NVIDIA GeForce RTX 4070 Ti",
                        "gpu_count": 1,
                        "cpu_model": "Intel Core i7-13700K",
                        "cpu_cores": 16,
                        "ram": 32768,
                        "vram": 12288,
                        "ssd": true
                    }
                ])";

                auto parsed_json = crow::json::load(jsonTemp);
                if (!parsed_json) {
                    return crow::response(400, "Invalid JSON format");
                }

                crow::json::wvalue response;
                response["result"] = parsed_json;
                return crow::response(200, response);
            }

            return crow::response(400, R"({"error": "No data"})");
        });

    std::uint16_t port = 8080;
    app.port(port).concurrency(2).run();
    eLog("Started api on port {}", port);
}
