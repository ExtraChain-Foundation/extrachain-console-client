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
#include <mutex>
#include <regex>
#include <QFile>

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

void run_api(ExtraChainNode* node, const std::string& api_token) {
    crow::SimpleApp app;
    std::string     token_session = api_token;
    eLog("API runned.");

    auto json_error = [](int code, const std::string& message) {
        crow::json::wvalue err;
        err["error"] = message;
        return crow::response(code, err);
    };

    auto load_mint_actor = []() -> std::optional<Actor<KeyPrivate>> {
        QFile f("minting_actor.json");
        if (!f.open(QIODevice::ReadOnly)) return std::nullopt;
        auto a = Actor<KeyPrivate>::fromJson(f.readAll());
        f.close();
        if (a.empty()) return std::nullopt;
        return a;
    };

    auto check_token_post = [&](const crow::json::rvalue& json) -> std::optional<crow::response> {
        if (!json.has("token")) return json_error(400, "token required");
        std::string token = std::string(json["token"].s());
        if (token.empty()) return json_error(400, "token is empty");
        if (token != token_session) return json_error(400, "token is not valid");
        return std::nullopt;
    };

    auto check_token_get = [&](const crow::request& req) -> std::optional<crow::response> {
        auto token_raw = req.url_params.get("token");
        if (!token_raw) return json_error(400, "token required");
        std::string token(token_raw);
        if (token.empty()) return json_error(400, "token is empty");
        if (token != token_session) return json_error(400, "token is not valid");
        return std::nullopt;
    };

    CROW_ROUTE(app, "/balance").methods("POST"_method)([&](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return json_error(400, "invalid json");
        if (auto err = check_token_post(json)) return std::move(*err);
        if (!json.has("actor_id")) return json_error(400, "actor_id required");

        std::string actorIdStr = json["actor_id"].s();
        auto        actor_id   = ActorId::create(actorIdStr);
        if (!actor_id.has_value()) return json_error(400, "invalid actor_id");

        TokenId tokenId("468faf2f1be6504a9a26f7f027f7e43380b0d77d");

        if (!node->actor_index()->exists(actor_id.value())) return json_error(404, "actor not found");

        eLog("[api] [POST] [balance] [actor_id: {}]", actorIdStr);

        std::map<std::pair<ActorId, TokenId>, BigNumberFloat> balances =
            node->dag()->calculate_actors_balance({ actor_id.value() });

        BigNumberFloat balance    = BigNumberFloat(0);
        auto           balanceKey = std::make_pair(actor_id.value(), tokenId);
        auto           it         = balances.find(balanceKey);
        if (it != balances.end()) {
            balance = it->second;
        }

        crow::json::wvalue response;
        response["actor_id"] = actor_id.value().to_string();
        response["balance"]  = balance.to_string();
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/transaction_by_hash_and_section_id")
        .methods("POST"_method)([&](const crow::request& req) {
            auto json = crow::json::load(req.body);
            if (!json) return json_error(400, "invalid json");
            if (auto err = check_token_post(json)) return std::move(*err);
            if (!json.has("hash")) return json_error(400, "hash required");
            if (!json.has("section_id")) return json_error(400, "section_id required");

            std::string hash          = json["hash"].s();
            int         sectionNumber = json["section_id"].i();
            auto        section       = node->dag()->read_section(SectionId(sectionNumber));
            if (!section.has_value()) return json_error(400, "section not found");

            auto transactions = section->transactions;
            if (transactions.empty()) return json_error(400, "list transactions is empty");

            eLog("[api] [POST] [transaction_by_hash_and_section_id] [hash: {}]", hash);

            auto it = std::find_if(transactions.begin(), transactions.end(), [&hash](const Transaction& t) {
                return t.hash() == hash;
            });

            if (it == transactions.end()) return json_error(400, "transaction not found");

            Transaction        tx = *it;
            crow::json::wvalue response;
            response["hash"]     = hash;
            response["sender"]   = tx.sender().to_string();
            response["receiver"] = tx.receiver().to_string();
            response["amount"]   = tx.amount().to_string();
            auto dateTime        = QDateTime::fromMSecsSinceEpoch(tx.timestamp());
            response["date"]     = dateTime.toString("dd/MM/yyyy").toStdString();
            response["time"]     = dateTime.toString("hh:mm:ss").toStdString();
            std::string typeTx;
            switch (tx.type()) {
            case TransactionType::Genesis:     typeTx = "genesis";       break;
            case TransactionType::Balance:     typeTx = "balance";       break;
            case TransactionType::Burn:        typeTx = "burn";          break;
            case TransactionType::InitContract: typeTx = "init_contract"; break;
            case TransactionType::Conversion:  typeTx = "conversion";    break;
            case TransactionType::Regular:     typeTx = "regular";       break;
            case TransactionType::Repeatable:  typeTx = "repeatable";    break;
            case TransactionType::Reward:      typeTx = "reward";        break;
            case TransactionType::Minting:     typeTx = "minting";       break;
            default:                                                       break;
            }
            response["type"] = typeTx;
            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/count_sections")
        .methods("GET"_method)([&](const crow::request& req) {
            if (auto err = check_token_get(req)) return std::move(*err);
            eLog("[api] [GET] [count_sections]");
            crow::json::wvalue response;
            response["count_sections"] = node->dag()->current_section().to_string();
            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/count_transactions_in_section")
        .methods("GET"_method)([&](const crow::request& req) {
            if (auto err = check_token_get(req)) return std::move(*err);

            auto number_section = req.url_params.get("number_section");
            if (!number_section) return json_error(400, "number_section required");

            int  section_number = std::stoi(number_section);
            auto section        = node->dag()->read_section(BigNumber(section_number));
            if (!section.has_value()) return json_error(400, "invalid number section");

            eLog("[api] [GET] [count_transactions_in_section] [number_section: {}]", number_section);

            crow::json::wvalue response;
            response["count_transactions"] = section->transactions.size();
            response["section_number"]     = number_section;
            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/have_rewards").methods("POST"_method)([&](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return json_error(400, "invalid json");
        if (auto err = check_token_post(json)) return std::move(*err);
        if (!json.has("actor_id")) return json_error(400, "actor_id required");

        std::string actorId  = json["actor_id"].s();
        std::string period   = json.has("period") ? json["period"].s() : std::string();
        long long   periodMs = parseTimeToMs(period);

        auto actor_id = ActorId::create(actorId);
        if (!actor_id.has_value()) return json_error(400, "invalid actor_id");

        auto now          = std::chrono::system_clock::now().time_since_epoch();
        auto cutoffTimeMs = static_cast<std::uint64_t>(now.count() - periodMs);

        bool hasRewards  = false;
        int  rewardCount = 0;

        // ChainIndex returns tx metadata directly — no section read needed.
        // We pull "received_by" since rewards arrive at the actor; no upper limit
        // on rows (rewards window is already bounded by cutoffTimeMs).
        const auto entries =
            node->dag()->chain_index().find_received_by(actor_id.value().to_string(), {}, 0, 10000);
        for (const auto& e : entries) {
            if (e.type != static_cast<int>(TransactionType::Reward)) continue;
            if (e.timestamp >= cutoffTimeMs) {
                hasRewards = true;
                rewardCount++;
            } else {
                // entries are timestamp-DESC, so first older-than-cutoff stops the scan
                break;
            }
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
        .methods("POST"_method)([&](const crow::request& req) {
            auto json = crow::json::load(req.body);
            if (!json) return json_error(400, "invalid json");
            if (auto err = check_token_post(json)) return std::move(*err);
            if (!json.has("actor_id")) return json_error(400, "actor_id required");

            std::string actorId  = json["actor_id"].s();
            auto        actor_id = ActorId::create(actorId);
            if (!actor_id.has_value()) return json_error(400, "invalid actor_id");

            auto raccoon_id = ActorId("46710a2d823c23db9fc2ac01e0f84212a8128373");

            auto search_result =
                Dfs::Tables::DirsFile::ActorSpace::search_file_by_folder_and_name(node->dfs()->get_db_instance(),
                                                                                  raccoon_id,
                                                                                  Dfs::Basic::TEMPLATE_VECTOR,
                                                                                  "RaccoonSubscription");
            if (!search_result.has_value()) return json_error(400, "can not find subscription");

            bool subscribeActive = search_result->state == Dfs::FileState::Ready;
            bool subscribed      = false;

            eLog("[api] [POST] [subscription_state] [actor_id: {}]", actorId);
            auto row = node->dfs()->read_vector_row(raccoon_id, search_result->file_id, actor_id->to_string());
            subscribed = row.has_value();

            crow::json::wvalue response;
            response["actor_id"]   = actor_id.value().to_string();
            response["active"]     = subscribeActive;
            response["subscribed"] = subscribed;
            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/get_actor")
        .methods("GET"_method)([&](const crow::request& req) {
            if (auto err = check_token_get(req)) return std::move(*err);

            auto id = req.url_params.get("id");
            if (!id) return json_error(400, "id required");

            auto actor_id = ActorId::create(id);
            if (!actor_id.has_value()) return json_error(400, "invalid actor_id");

            auto res = node->actor_index()->read_actor(actor_id.value());
            if (!res.has_value()) return json_error(400, "No actor");

            crow::json::wvalue response;
            response["public_key"] = Utils::to_base64(res.value().key().public_key());
            return crow::response(200, response);
        });

    CROW_ROUTE(app, "/verify_actor")
        .methods("GET"_method)([&](const crow::request& req) {
            if (auto err = check_token_get(req)) return std::move(*err);

            auto id      = req.url_params.get("id");
            auto sig_str = req.url_params.get("signature");
            if (!id) return json_error(400, "id required");
            if (!sig_str) return json_error(400, "signature required");

            auto actor_id = ActorId::create(id);
            if (!actor_id.has_value()) return json_error(400, "invalid actor_id");

            auto actor_data = node->actor_index()->read_actor(actor_id.value());
            if (!actor_data.has_value()) return json_error(400, "No actor");

            auto decoded_sig = Utils::from_base64<std::vector<std::uint8_t>>(sig_str);
            if (!decoded_sig) return json_error(400, "Base64 decoding failed");

            const auto& decoded = decoded_sig.value();
            if (decoded.size() != crypto_sign_BYTES) return json_error(400, "Invalid signature length");

            Signature signature;
            std::copy(decoded.begin(), decoded.end(), signature.begin());

            auto res = actor_data->key().verify(id, signature);
            if (!res.has_value()) return json_error(400, "verification failed");

            crow::json::wvalue response;
            response["result"] = res.value();
            return crow::response(200, response);
        });

    static std::mutex mint_mutex;
    CROW_ROUTE(app, "/mint").methods("POST"_method)([&](const crow::request& req) -> crow::response {
        try {
        auto json = crow::json::load(req.body);
        if (!json) return json_error(400, "invalid json");
        if (auto err = check_token_post(json)) return std::move(*err);
        if (!json.has("actor_id")) return json_error(400, "actor_id required");
        if (!json.has("amount")) return json_error(400, "amount required");

        std::string actorIdStr = json["actor_id"].s();
        auto        receiver   = ActorId::create(actorIdStr);
        if (!receiver.has_value()) return json_error(400, "invalid actor_id");

        if (!node->actor_index()->exists(receiver.value())) return json_error(404, "actor not found");

        std::string amountStr  = json["amount"].s();
        auto        amount_res = BigNumberFloat::create(amountStr);
        if (!amount_res.has_value()) return json_error(400, "invalid amount");
        BigNumberFloat amount = amount_res.value();
        if (amount <= 0) return json_error(400, "amount must be positive");
        if (amount > 5000) return json_error(400, "amount exceeds maximum of 5000");

        auto owner_opt = load_mint_actor();
        if (!owner_opt.has_value()) return json_error(500, "failed to load owner actor");
        auto owner_actor = owner_opt.value();

        Transaction tx;
        tx.set_sender(owner_actor.id());
        tx.set_receiver(receiver.value());
        tx.set_amount(amount);
        tx.set_token(TokenId("468faf2f1be6504a9a26f7f027f7e43380b0d77d"));
        tx.set_type(TransactionType::Minting);

        std::lock_guard<std::mutex> lock(mint_mutex);
        auto result = node->dag()->send_transaction(tx, owner_actor);
        if (!result.has_value()) {
            return json_error(500, "transaction failed: " + Utils::enum_value_name_value(result.error()));
        }

        // Update token_allocations dictionary: accumulate minted amount for this actor
        auto network_id = node->actor_index()->network_id();
        auto alloc_row  = Dfs::Tables::DirsFile::ActorSpace::search_file_by_folder_and_name(
            node->dfs()->get_db_instance(), network_id, Dfs::Basic::TEMPLATE_DICTIONARY, "token_allocations");
        if (alloc_row.has_value()) {
            std::string alloc_key = fmt::format("{}:{}", actorIdStr, tx.token().to_string());
            auto           current_str   = node->dfs()->read_dictionary(network_id, alloc_row->file_id, alloc_key);
            BigNumberFloat current_minted(0);
            if (current_str.has_value() && !current_str->empty()) {
                auto parsed = BigNumberFloat::create(*current_str);
                if (parsed.has_value())
                    current_minted = parsed.value();
            }
            current_minted += amount;
            node->dfs()->dictionary_set_value(network_id, alloc_row->file_id, alloc_key,
                                              current_minted.to_string(), network_id);
        } else {
            eWarning("[api] [mint] token_allocations dictionary not found, skipping freeze tracking");
        }

        eLog("[api] [POST] [mint] [receiver: {}] [amount: {}]", actorIdStr, amountStr);

        crow::json::wvalue response;
        response["hash"]     = result.value().hash();
        response["receiver"] = receiver.value().to_string();
        response["amount"]   = amountStr;
        return crow::response(200, response);
        } catch (const std::exception& e) {
            return json_error(500, fmt::format("internal error: {}", e.what()));
        } catch (...) {
            return json_error(500, "internal error");
        }
    });

    static std::mutex subscription_mutex;
    CROW_ROUTE(app, "/subscription_add").methods("POST"_method)([&](const crow::request& req) -> crow::response {
        try {
            auto json = crow::json::load(req.body);
            if (!json) return json_error(400, "invalid json");
            if (auto err = check_token_post(json)) return std::move(*err);
            if (!json.has("actor_id")) return json_error(400, "actor_id required");
            if (!json.has("until_ms")) return json_error(400, "until_ms required");

            std::string actorIdStr = json["actor_id"].s();
            auto        actor      = ActorId::create(actorIdStr);
            if (!actor.has_value()) return json_error(400, "invalid actor_id");
            if (!node->actor_index()->exists(actor.value())) return json_error(404, "actor not found");

            std::uint64_t until_ms = json["until_ms"].u();
            auto now_ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            if (until_ms <= now_ms) return json_error(400, "until_ms must be in the future");

            auto owner_opt = load_mint_actor();
            if (!owner_opt.has_value()) return json_error(500, "failed to load owner actor");
            auto owner_actor = owner_opt.value();

            auto sub_row = Dfs::Tables::DirsFile::ActorSpace::search_file_by_folder_and_name(
                node->dfs()->get_db_instance(), owner_actor.id(),
                Dfs::Basic::TEMPLATE_DICTIONARY, "SubscriptionsPay");
            if (!sub_row.has_value()) return json_error(500, "subscriptions dictionary not found");

            std::lock_guard<std::mutex> lock(subscription_mutex);

            node->dfs()->dictionary_set_value(owner_actor.id(), sub_row->file_id, actorIdStr,
                                              std::to_string(until_ms), owner_actor.id());

            eLog("[api] [POST] [subscription_add] [actor: {}] [until: {}]", actorIdStr, until_ms);

            crow::json::wvalue response;
            response["actor_id"] = actorIdStr;
            response["until_ms"] = until_ms;
            return crow::response(200, response);
        } catch (const std::exception& e) {
            return json_error(500, fmt::format("internal error: {}", e.what()));
        } catch (...) {
            return json_error(500, "internal error");
        }
    });

    CROW_ROUTE(app, "/subscription_check").methods("GET"_method)([&](const crow::request& req) -> crow::response {
        try {
            if (auto err = check_token_get(req)) return std::move(*err);

            auto id = req.url_params.get("actor_id");
            if (!id) return json_error(400, "actor_id required");

            auto actor = ActorId::create(id);
            if (!actor.has_value()) return json_error(400, "invalid actor_id");

            auto owner_opt = load_mint_actor();
            if (!owner_opt.has_value()) return json_error(500, "failed to load owner actor");
            auto owner_actor = owner_opt.value();

            auto sub_row = Dfs::Tables::DirsFile::ActorSpace::search_file_by_folder_and_name(
                node->dfs()->get_db_instance(), owner_actor.id(),
                Dfs::Basic::TEMPLATE_DICTIONARY, "SubscriptionsPay");
            if (!sub_row.has_value()) return json_error(500, "subscriptions dictionary not found");

            auto value = node->dfs()->read_dictionary(owner_actor.id(), sub_row->file_id, actor->to_string());

            std::uint64_t until_ms = 0;
            if (value.has_value() && !value->empty()) {
                try { until_ms = std::stoull(*value); } catch (...) {}
            }

            auto now_ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

            eLog("[api] [GET] [subscription_check] [actor: {}]", actor->to_string());

            crow::json::wvalue response;
            response["actor_id"]     = actor->to_string();
            response["until_ms"]     = until_ms;
            response["active"]       = until_ms > now_ms;
            response["remaining_ms"] = until_ms > now_ms ? until_ms - now_ms : 0;
            return crow::response(200, response);
        } catch (const std::exception& e) {
            return json_error(500, fmt::format("internal error: {}", e.what()));
        } catch (...) {
            return json_error(500, "internal error");
        }
    });

    std::uint16_t port = 17581;
    app.bindaddr("0.0.0.0").port(port).concurrency(2).run();
    eLog("Started api on port {}", port);
}
