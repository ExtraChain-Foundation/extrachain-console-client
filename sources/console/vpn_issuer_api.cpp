#include "console/vpn_issuer_api.h"

#include "vpn_grant_issuer.h"
#include "managers/extrachain_node.h"
#include "dfs/dfs_controller.h"
#include <QFile>
#include <QJsonDocument>

std::shared_ptr<VpnGrantIssuer> makeVpnGrantIssuer(ExtraChainNode* node, std::mutex& subscriptions) {
    const auto path     = qEnvironmentVariable("RACCOON_VPN_ISSUER_CONFIG");
    auto       settings = path.isEmpty() ? std::nullopt : vpnReadIssuerSettings(path);
    if (!settings) {
        return {};
    }
    auto issuer = std::make_shared<VpnGrantIssuer>(
        std::move(*settings),
        [node](const QString& name) -> QByteArray {
            const auto id = ActorId::create(name.toStdString());
            if (!id || id->is_zero() || id->toQString() != name) {
                return {};
            }
            const auto actor = node->actor_index()->read_actor(*id, ActorGetType::NoRequest);
            if (!actor || actor->id() != *id || actor->key().empty()) {
                return {};
            }
            const auto& key = actor->key().public_key();
            return QByteArray(reinterpret_cast<const char*>(key.data()), key.size());
        },
        [node, &subscriptions](const QString& account) -> std::optional<VpnPaidSubscription> {
            std::lock_guard guard(subscriptions);
            QFile           file("minting_actor.json");
            if (!file.open(QIODevice::ReadOnly) || file.size() > 16384) {
                return std::nullopt;
            }
            const auto owner = Actor<KeyPrivate>::fromJson(file.read(16385));
            if (owner.empty()) {
                return std::nullopt;
            }
            const auto row =
                Dfs::Tables::DirsFile::ActorSpace::search_file_by_folder_and_name(node->dfs()->get_db_instance(),
                                                                                  owner.id(),
                                                                                  Dfs::Basic::TEMPLATE_DICTIONARY,
                                                                                  "SubscriptionsPay");
            if (!row || row->state != Dfs::FileState::Ready) {
                return std::nullopt;
            }
            const auto record = node->dfs()->read_dictionary(owner.id(), row->file_id, account.toStdString());
            if (!record) {
                return std::nullopt;
            }
            const auto object = QJsonDocument::fromJson(QByteArray::fromStdString(*record)).object();
            const int  plan   = object["plan"].toInt(0);
            const auto until  = object["until_ms"].toInteger(0);
            if (object["plan"] != QJsonValue(plan) || object["until_ms"] != QJsonValue(until) || until <= 0
                || plan < 1 || plan > 4) {
                return VpnPaidSubscription {};
            }
            return VpnPaidSubscription { plan, until };
        });
    return issuer->open() ? issuer : nullptr;
}
