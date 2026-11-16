#pragma once

#include <memory>
#include <mutex>

class ExtraChainNode;
class VpnGrantIssuer;

std::shared_ptr<VpnGrantIssuer> makeVpnGrantIssuer(ExtraChainNode* node, std::mutex& subscriptions);
