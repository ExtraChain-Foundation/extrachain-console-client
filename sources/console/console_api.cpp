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

#include <algorithm>
#include "crow.h"
#include <chrono>
#include <mutex>
#include <regex>
#include <QFile>
#include <QStandardPaths>

#include "managers/extrachain_node.h"
#include "contracts/contract_manager.h"
#include "contracts/contract_codec.h"
#include "contracts/toolchain_registry.h"
#include "chain/dag.h"
#include "dfs/dfs_controller.h"
#include "managers/token_manager.h"
#include "utils/exc_utils.h"
#include <boost/describe.hpp>

struct SubscriptionRecord {
    std::uint64_t until_ms = 0;
    int           plan     = 0;
};
BOOST_DESCRIBE_STRUCT(SubscriptionRecord, (), (until_ms, plan))

namespace {
    std::expected<std::vector<std::uint8_t>, std::string> contract_arguments(const boost::json::object& json) {
        const auto* raw   = json.if_contains("arguments_base64");
        const auto* value = json.if_contains("arguments");
        if (raw != nullptr && value != nullptr) {
            return std::unexpected("arguments and arguments_base64 are mutually exclusive");
        }
        if (raw != nullptr) {
            if (!raw->is_string()) {
                return std::unexpected("arguments_base64 must be a string");
            }
            auto decoded = Utils::from_base64<std::vector<std::uint8_t>>(std::string(raw->as_string()));
            if (!decoded.has_value()) {
                return std::unexpected("arguments_base64 is invalid");
            }
            return *decoded;
        }
        if (value == nullptr) {
            return std::vector<std::uint8_t> {};
        }
        auto encoded = ExtraChain::Contracts::Codec::encode_json(boost::json::serialize(*value));
        if (!encoded.has_value()) {
            return std::unexpected(encoded.error().detail);
        }
        return *encoded;
    }

    std::optional<boost::json::object> request_object(const crow::request& request) {
        try {
            auto value = boost::json::parse(request.body);
            if (value.is_object()) {
                return value.as_object();
            }
        } catch (...) {
        }
        return std::nullopt;
    }

    std::string subscription_to_json(const SubscriptionRecord& rec) {
        return Json::serialize(rec);
    }

    SubscriptionRecord subscription_from_value(const std::string& raw) {
        SubscriptionRecord rec;
        if (raw.empty())
            return rec;

        // Legacy
        const bool looks_numeric = raw.find_first_not_of("0123456789") == std::string::npos;
        if (looks_numeric) {
            try {
                rec.until_ms = std::stoull(raw);
            } catch (...) {
            }
            return rec;
        }

        // New
        auto parsed = Json::deserialize<SubscriptionRecord>(raw);
        if (parsed.has_value()) {
            rec = parsed.value();
        } else {
            try {
                rec.until_ms = std::stoull(raw);
            } catch (...) {
            }
        }
        return rec;
    }
} // namespace

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
        if (!f.open(QIODevice::ReadOnly))
            return std::nullopt;
        auto a = Actor<KeyPrivate>::fromJson(f.readAll());
        f.close();
        if (a.empty())
            return std::nullopt;
        return a;
    };

    auto check_token_post = [&](const crow::json::rvalue& json) -> std::optional<crow::response> {
        if (!json.has("token"))
            return json_error(400, "token required");
        std::string token = std::string(json["token"].s());
        if (token.empty())
            return json_error(400, "token is empty");
        if (token != token_session)
            return json_error(400, "token is not valid");
        return std::nullopt;
    };

    auto check_token_get = [&](const crow::request& req) -> std::optional<crow::response> {
        auto token_raw = req.url_params.get("token");
        if (!token_raw)
            return json_error(400, "token required");
        std::string token(token_raw);
        if (token.empty())
            return json_error(400, "token is empty");
        if (token != token_session)
            return json_error(400, "token is not valid");
        return std::nullopt;
    };

    CROW_ROUTE(app, "/contract/list").methods("GET"_method)([&](const crow::request& req) {
        if (auto err = check_token_get(req))
            return std::move(*err);
        ExtraChain::Contracts::ContractCatalogFilter filter;
        if (const auto owner = req.url_params.get("owner_id"))
            filter.owner_id = owner;
        if (const auto kind = req.url_params.get("kind"))
            filter.kind = kind;
        if (const auto cursor = req.url_params.get("cursor"))
            filter.cursor = cursor;
        if (const auto limit = req.url_params.get("limit")) {
            try {
                filter.limit = std::stoull(limit);
            } catch (...) {
                return json_error(400, "limit is invalid");
            }
        }
        auto response = crow::response(200, Json::serialize(node->list_contracts(filter)));
        response.set_header("Content-Type", "application/json");
        return response;
    });

    CROW_ROUTE(app, "/token/create").methods("POST"_method)([&](const crow::request& req) {
        auto json   = crow::json::load(req.body);
        auto object = request_object(req);
        if (!json || !object.has_value())
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        const auto* name   = object->if_contains("name");
        const auto* ticker = object->if_contains("ticker");
        const auto* count  = object->if_contains("count");
        if (name == nullptr || ticker == nullptr || count == nullptr || !name->is_string() || !ticker->is_string()
            || !count->is_string())
            return json_error(400, "name, ticker and count required");
        const auto count_text = std::string(count->as_string());
        if (!std::regex_match(count_text, std::regex(R"([0-9]+(?:\.[0-9]+)?)")))
            return json_error(400, "count must be a positive decimal string");
        const auto wallet = node->account_controller()->current_wallet();
        if (wallet.empty())
            return json_error(409, "current wallet is not available");
        std::uint8_t decimals = 8;
        if (const auto* value = object->if_contains("decimals")) {
            if (!value->is_int64())
                return json_error(400, "decimals must be an integer");
            const auto parsed = value->as_int64();
            if (parsed < 0 || parsed > 18)
                return json_error(400, "decimals must be between 0 and 18");
            decimals = static_cast<std::uint8_t>(parsed);
        }
        const auto optional_string = [&](std::string_view key, std::string fallback = {}) {
            const auto* value = object->if_contains(key);
            return value != nullptr && value->is_string() ? std::string(value->as_string()) : std::move(fallback);
        };
        const std::string color      = optional_string("color", "#FA5448");
        const std::string predefined = optional_string("predefined_token_id");
        auto              created    = node->token_manager()->create_token(wallet.id(),
                                                           std::string(name->as_string()),
                                                           std::string(ticker->as_string()),
                                                           BigNumberFloat(count_text),
                                                           color,
                                                           predefined,
                                                           decimals);
        if (!created.has_value())
            return json_error(409, "token creation was rejected");
        crow::json::wvalue response;
        response["token_id"] = created->token_id.to_string();
        response["owner_id"] = created->owner_id.to_string();
        response["name"]     = created->name;
        response["ticker"]   = created->ticker;
        response["count"]    = created->count.to_string();
        return crow::response(202, response);
    });

    CROW_ROUTE(app, "/toolchain/status").methods("GET"_method)([&](const crow::request& req) {
        if (auto err = check_token_get(req))
            return std::move(*err);
        const auto manifest = node->toolchain_registry()->manifest();
        if (!manifest.has_value())
            return json_error(404, manifest.error().detail);
        auto response = crow::response(200, Json::serialize(*manifest));
        response.set_header("Content-Type", "application/json");
        return response;
    });

    CROW_ROUTE(app, "/toolchain/install").methods("POST"_method)([&](const crow::request& req) {
        auto json   = crow::json::load(req.body);
        auto object = request_object(req);
        if (!json || !object.has_value())
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        const auto first_install = object->if_contains("first_install");
        const auto allow_first = first_install != nullptr && first_install->is_bool() && first_install->as_bool();
        const auto root =
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/contract-toolchain";
        ExtraChain::Contracts::ToolchainInstaller installer(node, root);
        const auto                                result = installer.install_stable(allow_first);
        if (!result.has_value())
            return json_error(409, result.error().detail);
        auto response = crow::response(200, Json::serialize(result->manifest));
        response.set_header("Content-Type", "application/json");
        return response;
    });

    CROW_ROUTE(app, "/contract/build").methods("POST"_method)([&](const crow::request& req) {
        auto json   = crow::json::load(req.body);
        auto object = request_object(req);
        if (!json || !object.has_value())
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        const auto* project_name = object->if_contains("project_name");
        const auto* source       = object->if_contains("source");
        if (project_name == nullptr || source == nullptr || !project_name->is_string() || !source->is_string())
            return json_error(400, "project_name and source strings required");
        const auto root =
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/contract-toolchain";
        ExtraChain::Contracts::ToolchainInstaller installer(node, root);
        const auto                                result =
            installer.build_contract(QString::fromStdString(std::string(source->as_string())),
                                     QString::fromStdString(std::string(project_name->as_string())));
        if (!result.has_value())
            return json_error(409, result.error().detail);
        boost::json::object body;
        body["module"] = result->toStdString();
        auto response  = crow::response(200, boost::json::serialize(body));
        response.set_header("Content-Type", "application/json");
        return response;
    });

    CROW_ROUTE(app, "/toolchain/publish-package").methods("POST"_method)([&](const crow::request& req) {
        auto json   = crow::json::load(req.body);
        auto object = request_object(req);
        if (!json || !object.has_value())
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        const auto text = [&](std::string_view key) -> std::optional<std::string> {
            const auto* value = object->if_contains(key);
            if (value == nullptr || !value->is_string() || value->as_string().empty())
                return std::nullopt;
            return std::string(value->as_string());
        };
        const auto platform       = text("platform");
        const auto architecture   = text("architecture");
        const auto archive_format = text("archive_format");
        const auto version        = text("version");
        const auto data           = text("data_base64");
        if (!platform || !architecture || !archive_format || !version || !data)
            return json_error(400, "platform, architecture, archive_format, version and data_base64 required");
        const auto decoded = Utils::from_base64<std::vector<std::uint8_t>>(*data);
        if (!decoded.has_value() || decoded->empty())
            return json_error(400, "data_base64 is invalid");
        const auto result = node->toolchain_registry()->publish_package(*platform,
                                                                        *architecture,
                                                                        *archive_format,
                                                                        *version,
                                                                        *decoded);
        if (!result.has_value())
            return json_error(409, result.error().detail);
        auto response = crow::response(202, Json::serialize(*result));
        response.set_header("Content-Type", "application/json");
        return response;
    });

    CROW_ROUTE(app, "/toolchain/publish-manifest").methods("POST"_method)([&](const crow::request& req) {
        auto json   = crow::json::load(req.body);
        auto object = request_object(req);
        if (!json || !object.has_value())
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        const auto* manifest_value = object->if_contains("manifest");
        if (manifest_value == nullptr || !manifest_value->is_object())
            return json_error(400, "manifest object required");
        const auto manifest =
            Json::deserialize<ExtraChain::Contracts::ToolchainManifest>(boost::json::serialize(*manifest_value));
        if (!manifest.has_value())
            return json_error(400, "manifest is invalid");
        const auto result = node->toolchain_registry()->publish_manifest(*manifest);
        if (!result.has_value())
            return json_error(409, result.error().detail);
        return crow::response(202);
    });

    CROW_ROUTE(app, "/contract/deploy").methods("POST"_method)([&](const crow::request& req) {
        auto json   = crow::json::load(req.body);
        auto object = request_object(req);
        if (!json || !object.has_value())
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        if (!json.has("kind") || !json.has("module_base64")) {
            return json_error(400, "kind and module_base64 required");
        }
        auto module    = Utils::from_base64<std::vector<std::uint8_t>>(std::string(json["module_base64"].s()));
        auto arguments = contract_arguments(*object);
        if (!module.has_value() || module->empty() || !arguments.has_value()) {
            return json_error(400, "invalid module or arguments");
        }
        auto transaction = node->submit_contract_deploy(std::string(json["kind"].s()), *module, *arguments);
        if (!transaction.has_value())
            return json_error(409, transaction.error().detail);

        crow::json::wvalue response;
        response["contract_id"]      = transaction->receiver().to_string();
        response["transaction_hash"] = transaction->hash();
        response["section"]          = transaction->section().to_string();
        return crow::response(202, response);
    });

    CROW_ROUTE(app, "/contract/call").methods("POST"_method)([&](const crow::request& req) {
        auto json   = crow::json::load(req.body);
        auto object = request_object(req);
        if (!json || !object.has_value())
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        if (!json.has("contract_id") || !json.has("method")) {
            return json_error(400, "contract_id and method required");
        }
        auto contract_id = ActorId::create(std::string(json["contract_id"].s()));
        if (!contract_id.has_value())
            return json_error(400, "invalid contract_id");
        auto arguments = contract_arguments(*object);
        if (!arguments.has_value())
            return json_error(400, arguments.error());
        auto transaction = node->submit_contract_call(*contract_id, std::string(json["method"].s()), *arguments);
        if (!transaction.has_value())
            return json_error(409, transaction.error().detail);

        crow::json::wvalue response;
        response["transaction_hash"] = transaction->hash();
        response["section"]          = transaction->section().to_string();
        return crow::response(202, response);
    });

    CROW_ROUTE(app, "/contract/query").methods("POST"_method)([&](const crow::request& req) {
        auto json   = crow::json::load(req.body);
        auto object = request_object(req);
        if (!json || !object.has_value())
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        if (!json.has("contract_id") || !json.has("method")) {
            return json_error(400, "contract_id and method required");
        }
        auto contract_id = ActorId::create(std::string(json["contract_id"].s()));
        if (!contract_id.has_value())
            return json_error(400, "invalid contract_id");
        auto arguments = contract_arguments(*object);
        if (!arguments.has_value())
            return json_error(400, arguments.error());
        auto receipt = node->query_contract(*contract_id, std::string(json["method"].s()), *arguments);
        if (!receipt.has_value())
            return json_error(409, receipt.error().detail);

        boost::json::object response;
        response["data_base64"] = Utils::to_base64(receipt->data);
        if (const auto data_json = ExtraChain::Contracts::Codec::decode_json(receipt->data);
            data_json.has_value()) {
            response["data"] = boost::json::parse(*data_json);
        }
        response["state_hash"] = receipt->state_hash;
        response["version"]    = receipt->version;
        response["revision"]   = receipt->revision;
        auto result            = crow::response(200, boost::json::serialize(response));
        result.set_header("Content-Type", "application/json");
        return result;
    });

    CROW_ROUTE(app, "/contract/inspect").methods("GET"_method)([&](const crow::request& req) {
        if (auto err = check_token_get(req))
            return std::move(*err);
        auto contract_id_raw = req.url_params.get("contract_id");
        if (!contract_id_raw)
            return json_error(400, "contract_id required");
        auto contract_id = ActorId::create(std::string(contract_id_raw));
        if (!contract_id.has_value())
            return json_error(400, "invalid contract_id");
        auto record = node->contract_manager()->inspect(contract_id->to_string());
        if (!record.has_value())
            return json_error(404, record.error().detail);
        const auto& version  = record->versions.at(record->active_version - 1);
        const auto& revision = version.revisions.back();

        crow::json::wvalue response;
        response["contract_id"]                 = record->contract_id;
        response["owner_id"]                    = record->owner_id;
        response["kind"]                        = record->kind;
        response["version"]                     = version.version;
        response["revision"]                    = revision.revision;
        response["module_hash"]                 = version.module_hash;
        response["state_hash"]                  = revision.state_hash;
        response["transaction_hash"]            = revision.transaction_hash;
        response["checkpoint_revision"]         = revision.checkpoint_revision;
        response["checkpoint_section"]          = revision.checkpoint_block;
        response["checkpoint_state_hash"]       = revision.checkpoint_hash;
        response["checkpoint_transaction_hash"] = revision.checkpoint_transaction_hash;
        response["replay_depth"]                = revision.revision - revision.checkpoint_revision;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/contract/upgrade").methods("POST"_method)([&](const crow::request& req) {
        auto json   = crow::json::load(req.body);
        auto object = request_object(req);
        if (!json || !object.has_value())
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        if (!json.has("contract_id") || !json.has("module_base64")) {
            return json_error(400, "contract_id and module_base64 required");
        }
        auto contract_id = ActorId::create(std::string(json["contract_id"].s()));
        if (!contract_id.has_value())
            return json_error(400, "invalid contract_id");
        auto module    = Utils::from_base64<std::vector<std::uint8_t>>(std::string(json["module_base64"].s()));
        auto arguments = contract_arguments(*object);
        if (!module.has_value() || module->empty() || !arguments.has_value()) {
            return json_error(400, "invalid module or arguments");
        }
        auto transaction = node->submit_contract_upgrade(*contract_id, *module, *arguments);
        if (!transaction.has_value())
            return json_error(409, transaction.error().detail);

        crow::json::wvalue response;
        response["transaction_hash"] = transaction->hash();
        response["section"]          = transaction->section().to_string();
        return crow::response(202, response);
    });

    CROW_ROUTE(app, "/balance").methods("POST"_method)([&](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json)
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        if (!json.has("actor_id"))
            return json_error(400, "actor_id required");

        std::string actorIdStr = json["actor_id"].s();
        auto        actor_id   = ActorId::create(actorIdStr);
        if (!actor_id.has_value())
            return json_error(400, "invalid actor_id");

        TokenId tokenId("468faf2f1be6504a9a26f7f027f7e43380b0d77d");

        if (!node->actor_index()->exists(actor_id.value()))
            return json_error(404, "actor not found");

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

    CROW_ROUTE(app, "/transaction_by_hash_and_section_id").methods("POST"_method)([&](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json)
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        if (!json.has("hash"))
            return json_error(400, "hash required");
        if (!json.has("section_id"))
            return json_error(400, "section_id required");

        std::string hash          = json["hash"].s();
        int         sectionNumber = json["section_id"].i();
        auto        section       = node->dag()->read_section(SectionId(sectionNumber));
        if (!section.has_value())
            return json_error(400, "section not found");

        auto transactions = section->transactions;
        if (transactions.empty())
            return json_error(400, "list transactions is empty");

        eLog("[api] [POST] [transaction_by_hash_and_section_id] [hash: {}]", hash);

        auto it = std::find_if(transactions.begin(), transactions.end(), [&hash](const Transaction& t) {
            return t.hash() == hash;
        });

        if (it == transactions.end())
            return json_error(400, "transaction not found");

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
        case TransactionType::Minting:
            typeTx = "minting";
            break;
        default:
            break;
        }
        response["type"] = typeTx;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/count_sections").methods("GET"_method)([&](const crow::request& req) {
        if (auto err = check_token_get(req))
            return std::move(*err);
        eLog("[api] [GET] [count_sections]");
        crow::json::wvalue response;
        response["count_sections"] = node->dag()->current_section().to_string();
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/count_transactions_in_section").methods("GET"_method)([&](const crow::request& req) {
        if (auto err = check_token_get(req))
            return std::move(*err);

        auto number_section = req.url_params.get("number_section");
        if (!number_section)
            return json_error(400, "number_section required");

        int  section_number = std::stoi(number_section);
        auto section        = node->dag()->read_section(BigNumber(section_number));
        if (!section.has_value())
            return json_error(400, "invalid number section");

        eLog("[api] [GET] [count_transactions_in_section] [number_section: {}]", number_section);

        crow::json::wvalue response;
        response["count_transactions"] = section->transactions.size();
        response["section_number"]     = number_section;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/have_rewards").methods("POST"_method)([&](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json)
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        if (!json.has("actor_id"))
            return json_error(400, "actor_id required");

        std::string actorId  = json["actor_id"].s();
        std::string period   = json.has("period") ? json["period"].s() : std::string();
        long long   periodMs = parseTimeToMs(period);

        auto actor_id = ActorId::create(actorId);
        if (!actor_id.has_value())
            return json_error(400, "invalid actor_id");

        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
        const auto cutoffTimeMs = static_cast<std::uint64_t>(std::max<std::int64_t>(0, nowMs - periodMs));

        const auto* chainIndex = node->dag()->chain_index();
        if (!chainIndex)
            return json_error(503, "chain index is disabled");

        const auto rewardCount =
            chainIndex->count_for_actor_by_type_since(actor_id->to_string(),
                                                      static_cast<int>(TransactionType::Reward),
                                                      cutoffTimeMs);
        const bool hasRewards = rewardCount > 0;

        crow::json::wvalue response;
        response["actor_id"]     = actor_id.value().to_string();
        response["has_rewards"]  = hasRewards;
        response["reward_count"] = rewardCount;
        response["period_ms"]    = periodMs;
        response["period_str"]   = period.empty() ? "1d" : period;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/subscription_state").methods("POST"_method)([&](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json)
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        if (!json.has("actor_id"))
            return json_error(400, "actor_id required");

        std::string actorId  = json["actor_id"].s();
        auto        actor_id = ActorId::create(actorId);
        if (!actor_id.has_value())
            return json_error(400, "invalid actor_id");

        auto raccoon_id = ActorId("46710a2d823c23db9fc2ac01e0f84212a8128373");

        auto search_result =
            Dfs::Tables::DirsFile::ActorSpace::search_file_by_folder_and_name(node->dfs()->get_db_instance(),
                                                                              raccoon_id,
                                                                              Dfs::Basic::TEMPLATE_VECTOR,
                                                                              "RaccoonSubscription");
        if (!search_result.has_value())
            return json_error(400, "can not find subscription");

        bool subscribeActive = search_result->state == Dfs::FileState::Ready;
        bool subscribed      = false;

        eLog("[api] [POST] [subscription_state] [actor_id: {}]", actorId);
        auto row   = node->dfs()->read_vector_row(raccoon_id, search_result->file_id, actor_id->to_string());
        subscribed = row.has_value();

        crow::json::wvalue response;
        response["actor_id"]   = actor_id.value().to_string();
        response["active"]     = subscribeActive;
        response["subscribed"] = subscribed;
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/get_actor").methods("GET"_method)([&](const crow::request& req) {
        if (auto err = check_token_get(req))
            return std::move(*err);

        auto id = req.url_params.get("id");
        if (!id)
            return json_error(400, "id required");

        auto actor_id = ActorId::create(id);
        if (!actor_id.has_value())
            return json_error(400, "invalid actor_id");

        auto res = node->actor_index()->read_actor(actor_id.value());
        if (!res.has_value())
            return json_error(400, "No actor");

        crow::json::wvalue response;
        response["public_key"] = Utils::to_base64(res.value().key().public_key());
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/verify_actor").methods("GET"_method)([&](const crow::request& req) {
        if (auto err = check_token_get(req))
            return std::move(*err);

        auto id      = req.url_params.get("id");
        auto sig_str = req.url_params.get("signature");
        if (!id)
            return json_error(400, "id required");
        if (!sig_str)
            return json_error(400, "signature required");

        auto actor_id = ActorId::create(id);
        if (!actor_id.has_value())
            return json_error(400, "invalid actor_id");

        auto actor_data = node->actor_index()->read_actor(actor_id.value());
        if (!actor_data.has_value())
            return json_error(400, "No actor");

        auto decoded_sig = Utils::from_base64<std::vector<std::uint8_t>>(sig_str);
        if (!decoded_sig)
            return json_error(400, "Base64 decoding failed");

        const auto& decoded = decoded_sig.value();
        if (decoded.size() != crypto_sign_BYTES)
            return json_error(400, "Invalid signature length");

        Signature signature;
        std::copy(decoded.begin(), decoded.end(), signature.begin());

        auto res = actor_data->key().verify(id, signature);
        if (!res.has_value())
            return json_error(400, "verification failed");

        crow::json::wvalue response;
        response["result"] = res.value();
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/verify_data").methods("POST"_method)([&](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json)
            return json_error(400, "invalid json");
        if (auto err = check_token_post(json))
            return std::move(*err);
        if (!json.has("id"))
            return json_error(400, "id required");
        if (!json.has("data"))
            return json_error(400, "data required");
        if (!json.has("signature"))
            return json_error(400, "signature required");

        std::string id_str   = json["id"].s();
        std::string data_str = json["data"].s();
        std::string sig_str  = json["signature"].s();

        auto actor_id = ActorId::create(id_str);
        if (!actor_id.has_value())
            return json_error(400, "invalid actor_id");

        auto actor_data = node->actor_index()->read_actor(actor_id.value());
        if (!actor_data.has_value())
            return json_error(400, "No actor");

        auto decoded_sig = Utils::from_base64<std::vector<std::uint8_t>>(sig_str);
        if (!decoded_sig)
            return json_error(400, "Base64 decoding failed");

        const auto& decoded = decoded_sig.value();
        if (decoded.size() != crypto_sign_BYTES)
            return json_error(400, "Invalid signature length");

        Signature signature;
        std::copy(decoded.begin(), decoded.end(), signature.begin());

        auto res = actor_data->key().verify(data_str, signature);
        if (!res.has_value())
            return json_error(400, "verification failed");

        crow::json::wvalue response;
        response["result"] = res.value();
        return crow::response(200, response);
    });

    static std::mutex mint_mutex;
    CROW_ROUTE(app, "/mint").methods("POST"_method)([&](const crow::request& req) -> crow::response {
        try {
            auto json = crow::json::load(req.body);
            if (!json)
                return json_error(400, "invalid json");
            if (auto err = check_token_post(json))
                return std::move(*err);
            if (!json.has("actor_id"))
                return json_error(400, "actor_id required");
            if (!json.has("amount"))
                return json_error(400, "amount required");

            std::string actorIdStr = json["actor_id"].s();
            auto        receiver   = ActorId::create(actorIdStr);
            if (!receiver.has_value())
                return json_error(400, "invalid actor_id");

            if (!node->actor_index()->exists(receiver.value()))
                return json_error(404, "actor not found");

            std::string amountStr  = json["amount"].s();
            auto        amount_res = BigNumberFloat::create(amountStr);
            if (!amount_res.has_value())
                return json_error(400, "invalid amount");
            BigNumberFloat amount = amount_res.value();
            if (amount <= 0)
                return json_error(400, "amount must be positive");
            if (amount > 5000)
                return json_error(400, "amount exceeds maximum of 5000");

            auto owner_opt = load_mint_actor();
            if (!owner_opt.has_value())
                return json_error(500, "failed to load owner actor");
            auto owner_actor = owner_opt.value();

            Transaction tx;
            tx.set_sender(owner_actor.id());
            tx.set_receiver(receiver.value());
            tx.set_amount(amount);
            tx.set_token(TokenId("468faf2f1be6504a9a26f7f027f7e43380b0d77d"));
            tx.set_type(TransactionType::Minting);

            std::lock_guard<std::mutex> lock(mint_mutex);
            auto                        result = node->dag()->send_transaction(tx, owner_actor);
            if (!result.has_value()) {
                return json_error(500, "transaction failed: " + Utils::enum_value_name_value(result.error()));
            }

            // Update token_allocations dictionary: accumulate minted amount for this actor
            auto network_id = node->actor_index()->network_id();
            auto alloc_row =
                Dfs::Tables::DirsFile::ActorSpace::search_file_by_folder_and_name(node->dfs()->get_db_instance(),
                                                                                  network_id,
                                                                                  Dfs::Basic::TEMPLATE_DICTIONARY,
                                                                                  "token_allocations");
            if (alloc_row.has_value()) {
                std::string alloc_key   = fmt::format("{}:{}", actorIdStr, tx.token().to_string());
                auto        current_str = node->dfs()->read_dictionary(network_id, alloc_row->file_id, alloc_key);
                BigNumberFloat current_minted(0);
                if (current_str.has_value() && !current_str->empty()) {
                    auto parsed = BigNumberFloat::create(*current_str);
                    if (parsed.has_value())
                        current_minted = parsed.value();
                }
                current_minted += amount;
                node->dfs()->dictionary_set_value(network_id,
                                                  alloc_row->file_id,
                                                  alloc_key,
                                                  current_minted.to_string(),
                                                  network_id);
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
            if (!json)
                return json_error(400, "invalid json");
            if (auto err = check_token_post(json))
                return std::move(*err);
            if (!json.has("actor_id"))
                return json_error(400, "actor_id required");
            if (!json.has("until_ms"))
                return json_error(400, "until_ms required");

            std::string actorIdStr = json["actor_id"].s();
            auto        actor      = ActorId::create(actorIdStr);
            if (!actor.has_value())
                return json_error(400, "invalid actor_id");
            if (!node->actor_index()->exists(actor.value()))
                return json_error(404, "actor not found");

            std::uint64_t until_ms = json["until_ms"].u();
            auto now_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                         std::chrono::system_clock::now().time_since_epoch())
                                                         .count());
            if (until_ms <= now_ms)
                return json_error(400, "until_ms must be in the future");

            SubscriptionRecord rec;
            rec.until_ms = until_ms;
            if (json.has("plan"))
                rec.plan = static_cast<int>(json["plan"].i());

            auto owner_opt = load_mint_actor();
            if (!owner_opt.has_value())
                return json_error(500, "failed to load owner actor");
            auto owner_actor = owner_opt.value();

            auto sub_row =
                Dfs::Tables::DirsFile::ActorSpace::search_file_by_folder_and_name(node->dfs()->get_db_instance(),
                                                                                  owner_actor.id(),
                                                                                  Dfs::Basic::TEMPLATE_DICTIONARY,
                                                                                  "SubscriptionsPay");
            if (!sub_row.has_value())
                return json_error(500, "subscriptions dictionary not found");

            std::lock_guard<std::mutex> lock(subscription_mutex);

            node->dfs()->dictionary_set_value(owner_actor.id(),
                                              sub_row->file_id,
                                              actorIdStr,
                                              subscription_to_json(rec),
                                              owner_actor.id());

            eLog("[api] [POST] [subscription_add] [actor: {}] [until: {}] [plan: {}]",
                 actorIdStr,
                 until_ms,
                 rec.plan);

            crow::json::wvalue response;
            response["actor_id"] = actorIdStr;
            response["until_ms"] = until_ms;
            if (rec.plan != 0)
                response["plan"] = rec.plan;
            return crow::response(200, response);
        } catch (const std::exception& e) {
            return json_error(500, fmt::format("internal error: {}", e.what()));
        } catch (...) {
            return json_error(500, "internal error");
        }
    });

    CROW_ROUTE(app, "/subscription_check").methods("GET"_method)([&](const crow::request& req) -> crow::response {
        try {
            if (auto err = check_token_get(req))
                return std::move(*err);

            auto id = req.url_params.get("actor_id");
            if (!id)
                return json_error(400, "actor_id required");

            auto actor = ActorId::create(id);
            if (!actor.has_value())
                return json_error(400, "invalid actor_id");

            auto owner_opt = load_mint_actor();
            if (!owner_opt.has_value())
                return json_error(500, "failed to load owner actor");
            auto owner_actor = owner_opt.value();

            auto sub_row =
                Dfs::Tables::DirsFile::ActorSpace::search_file_by_folder_and_name(node->dfs()->get_db_instance(),
                                                                                  owner_actor.id(),
                                                                                  Dfs::Basic::TEMPLATE_DICTIONARY,
                                                                                  "SubscriptionsPay");
            if (!sub_row.has_value())
                return json_error(500, "subscriptions dictionary not found");

            auto value = node->dfs()->read_dictionary(owner_actor.id(), sub_row->file_id, actor->to_string());

            SubscriptionRecord rec;
            if (value.has_value())
                rec = subscription_from_value(*value);
            std::uint64_t until_ms = rec.until_ms;

            auto now_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                         std::chrono::system_clock::now().time_since_epoch())
                                                         .count());

            eLog("[api] [GET] [subscription_check] [actor: {}]", actor->to_string());

            crow::json::wvalue response;
            response["actor_id"]     = actor->to_string();
            response["until_ms"]     = until_ms;
            response["active"]       = until_ms > now_ms;
            response["remaining_ms"] = until_ms > now_ms ? until_ms - now_ms : 0;
            if (rec.plan != 0)
                response["plan"] = rec.plan;
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
