# Optional Raccoon VPN grant issuer

The subscription writer is `sources/console/console_api.cpp`:
`POST /subscription_add` writes `{until_ms, plan}` into the owner's
`SubscriptionsPay` dictionary under the main actor's canonical ID.
`GET /subscription_check` reads that record. Administrative routes retain their
existing API-token authentication. A failed dictionary write now returns 503
instead of reporting success; unsupported plan codes are rejected.

## Enablement

Build with `-DEXTRACHAIN_VPN_ISSUER=ON`. The default is OFF. This links the
standalone sibling `raccoon-core/control` library, without the VPN engine or UI.
`RACCOON_CONTROL_DIR` can select another checkout of that component.

At startup, `RACCOON_VPN_ISSUER_CONFIG` selects a private local JSON file:

```json
{
  "issuer": "billing-key-v1",
  "signingSeed": "<canonical base64url of a dedicated random 32-byte signing seed>",
  "database": "vpn-issuance.sqlite",
  "maxOutstandingPerAccount": 8,
  "audiences": [
    {"actor": "<approved exit actor>", "country": "DE", "role": "exit", "plans": [2, 4]},
    {"actor": "<approved entry actor>", "country": "FR", "role": "entry", "plans": [1, 2, 3, 4]}
  ]
}
```

The values above are placeholders, not usable credentials or production policy.
Relative database paths resolve against the configuration directory. Keep the
file and database private to the service. The seed must be dedicated to this
issuer, never a wallet recovery seed or the minting actor's key. The corresponding
Ed25519 public key must be pinned by clients and VPN nodes through a trusted
distribution mechanism; the request cannot choose a verification key.

Use the authoritative writer instance with its existing local mint-owner profile
and `minting_actor.json`. Issuance reads only a Ready `SubscriptionsPay` dictionary
and an already-known canonical actor key; it does not request missing actors from
P2P. Missing/unready data returns 503. Unknown, legacy plan 0 and expired records
do not produce paid grants. Run behind the authenticated TLS endpoint used for
the payment service; the bodies are additionally sealed to the pinned issuer key.
Missing/invalid issuer configuration leaves the new route unavailable (503).
No service or deployment is enabled automatically by these source changes.

## POST /vpn/access-grant

This client route does not accept or require the administrative API token.
It accepts the existing `controlVersion: 1` sealed-box envelope with the signature
domain `grant-issue-v1`, signed by main and addressed to the issuer. Inside:

```json
{
  "version": 1,
  "subject": "vpn-session:<temporary Ed25519 public key in base64url>",
  "audience": "<approved VPN node actor>",
  "role": "exit",
  "country": "DE",
  "issuedAt": "<UTC milliseconds as a decimal string>",
  "expiresAt": "<UTC milliseconds as a decimal string>",
  "nonce": "<fresh 32-byte nonce in canonical base64url>"
}
```

The signed lifetime must be positive and at most 60 seconds. Future and expired
proofs are rejected. No `actor_id`, tariff, subscription expiry or other extra
claim is accepted. The authenticated envelope sender selects the subscription.
The node, role and location must match an operator-approved audience that allows
the authoritative tariff.

A 200 reply is sealed to main using `grant-response-v1`, names the configured
issuer and binds the exact request digest. Its body is `{version: 1, grant: ...}`.
Only the grant goes to the eventual VPN node: it contains the temporary subject,
tariff, node/role/location, random grant ID and deadlines, not main/system/device
identity. Admission is at most five minutes and the session at most 24 hours;
both deadlines are capped at the current subscription expiry.

The client must retain and retry the identical encrypted request. The durable
SQLite WAL cache returns the same grant across retries and process restarts.
An altered encrypted request is a separate issuance and consumes quota. A reduced
subscription invalidates an oversized cached result (409), rather than extending
that entitlement. All replies use `Cache-Control: no-store`; request bodies and
identity mappings are not logged by the new handler.

| Status | Meaning |
| --- | --- |
| 400 | Malformed, expired or future request |
| 401 | Main ownership proof missing or invalid |
| 403 | Subscription inactive/unknown or requested scope disallowed |
| 409 | Cached grant no longer fits the current subscription |
| 413 | Request body exceeds 64 KiB |
| 429 | Outstanding issuance quota or global cache capacity reached |
| 503 | Configuration, trusted data, clock or durable storage unavailable |

The cache holds at most 10,000 unexpired grants; only expired entries are pruned.
Account buckets are keyed by a signing-key-dependent HMAC. A database error or
corruption fails closed. Do not reset/restore old issuance state to bypass quotas.
`maxOutstandingPerAccount` limits grants in their admission window, not active
devices: independent replicas do not share this local quota. Global concurrency,
revocation/renewal, key rotation and live-session release require further work.

## Verification and remaining work

`raccoon-core/tests/vpn_grant_issuer` uses synthetic identities and subscriptions
to exercise ownership, scope/tariff checks, expiry, immutable retries, concurrency,
quota retention and corrupted storage. The common admission/control suites still
exercise node verification and replay protection. These are not real purchases
or a production issuer qualification.

The live VPN connector still uses protocol 2. Client issuance requests, private
entry-to-exit forwarding, node consumption, P2P isolation, native renewal/close
and Windows/Android runtime qualification remain to be integrated before rollout.
