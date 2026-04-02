# ExtraChain Console API

REST API for interacting with the ExtraChain node.

**Base URL:** `http://<host>:17581`  
**Authentication:** All endpoints require an `token` parameter (POST body or query string).

To start the API, pass `--api-token` on launch:
```bash
./extrachain-console --api-token <your_token>
```

**Error format:** All error responses use the following JSON structure:
```json
{
  "error": "error message"
}
```

---

## Endpoints

### POST /balance

Returns the ROCC token balance for a given actor.

**Body:**
```json
{
  "actor_id": "<actor id>",
  "token": "<api token>"
}
```

**Response (200):**
```json
{
  "actor_id": "769cfeb282cd66f46cbb02d22e5e482e98ea92d8",
  "balance": "160"
}
```

**Errors:**
| Code | Reason |
|------|--------|
| 400 | `actor_id required` / `invalid actor_id` / `token is not valid` |
| 404 | `actor not found` |

**Example:**
```bash
curl -X POST http://<host>:17581/balance \
  -H "Content-Type: application/json" \
  -d '{"actor_id": "769cfeb282cd66f46cbb02d22e5e482e98ea92d8", "token": "<api_token>"}'
```

---

### POST /mint

Mints ROCC tokens to a specified actor. Owner-signed, no balance required.

**Limits:** `amount` must be between 0 and 5000 per request.

**Body:**
```json
{
  "actor_id": "<receiver actor id>",
  "amount": "<amount>",
  "token": "<api token>"
}
```

**Response (200):**
```json
{
  "hash": "ecd5618e1929fd0076b53125d39d4cf90ea9133d9405ceeef53b294992bcd1b1",
  "receiver": "769cfeb282cd66f46cbb02d22e5e482e98ea92d8",
  "amount": "10"
}
```

**Errors:**
| Code | Reason |
|------|--------|
| 400 | `actor_id required` / `amount required` / `token is not valid` |
| 400 | `invalid actor_id` / `invalid amount` / `amount must be positive` |
| 400 | `amount exceeds maximum of 5000` |
| 404 | `actor not found` |
| 500 | `transaction failed: <reason>` |

**Example:**
```bash
curl -X POST http://<host>:17581/mint \
  -H "Content-Type: application/json" \
  -d '{"actor_id": "769cfeb282cd66f46cbb02d22e5e482e98ea92d8", "amount": "100", "token": "<api_token>"}'
```

---

### GET /get_actor

Returns the public key of an actor by ID. Can be used to check if an actor exists in the network.

**Query parameters:**
| Parameter | Required | Description |
|-----------|----------|-------------|
| `id` | yes | Actor ID |
| `token` | yes | API token |

**Response (200):**
```json
{
  "public_key": "bkviwxUKHRoLr-XxGuH7TJqixDRho1RVOZwBvO1_x34"
}
```

**Errors:**
| Code | Reason |
|------|--------|
| 400 | `id required` / `invalid actor_id` / `token is not valid` |
| 400 | `No actor` — actor not registered in the network |

**Example:**
```bash
curl "http://<host>:17581/get_actor?id=aa8d056f1165e5455400be99989e33edbeb9c090&token=<api_token>"
```

---

### GET /verify_actor

Verifies a Ed25519 signature against an actor's public key.

**Query parameters:**
| Parameter | Required | Description |
|-----------|----------|-------------|
| `id` | yes | Actor ID |
| `signature` | yes | Base64-encoded signature |
| `token` | yes | API token |

**Response (200):**
```json
{
  "result": true
}
```

**Errors:**
| Code | Reason |
|------|--------|
| 400 | `id required` / `signature required` / `token is not valid` |
| 400 | `invalid actor_id` / `No actor` |
| 400 | `Base64 decoding failed` / `Invalid signature length` / `verification failed` |

**Example:**
```bash
curl "http://<host>:17581/verify_actor?id=aa8d056f1165e5455400be99989e33edbeb9c090&signature=<base64_sig>&token=<api_token>"
```

---

### GET /count_sections

Returns the current section count of the DAG.

**Query parameters:**
| Parameter | Required | Description |
|-----------|----------|-------------|
| `token` | yes | API token |

**Response (200):**
```json
{
  "count_sections": "1042"
}
```

**Example:**
```bash
curl "http://<host>:17581/count_sections?token=<api_token>"
```

---

### GET /count_transactions_in_section

Returns the number of transactions in a given section.

**Query parameters:**
| Parameter | Required | Description |
|-----------|----------|-------------|
| `number_section` | yes | Section number |
| `token` | yes | API token |

**Response (200):**
```json
{
  "section_number": "42",
  "count_transactions": 7
}
```

**Errors:**
| Code | Reason |
|------|--------|
| 400 | `number_section required` / `invalid number section` / `token is not valid` |

**Example:**
```bash
curl "http://<host>:17581/count_transactions_in_section?number_section=42&token=<api_token>"
```

---

### POST /transaction_by_hash_and_section_id

Returns transaction details by hash and section ID.

**Body:**
```json
{
  "hash": "<transaction hash>",
  "section_id": 42,
  "token": "<api token>"
}
```

**Response (200):**
```json
{
  "hash": "ecd5618e...",
  "sender": "46710a2d823c23db9fc2ac01e0f84212a8128373",
  "receiver": "769cfeb282cd66f46cbb02d22e5e482e98ea92d8",
  "amount": "10",
  "date": "02/04/2026",
  "time": "14:32:11",
  "type": "minting"
}
```

**Transaction types:**

| Value | Type |
|-------|------|
| `genesis` | Genesis |
| `regular` | Regular transfer |
| `repeatable` | Repeatable transfer |
| `init_contract` | Contract initialization |
| `reward` | Mining reward |
| `burn` | Token burn |
| `conversion` | Token conversion |
| `minting` | Token minting |
| `balance` | Balance snapshot |

**Errors:**
| Code | Reason |
|------|--------|
| 400 | `hash required` / `section_id required` / `token is not valid` |
| 400 | `section not found` / `list transactions is empty` / `transaction not found` |

**Example:**
```bash
curl -X POST http://<host>:17581/transaction_by_hash_and_section_id \
  -H "Content-Type: application/json" \
  -d '{"hash": "ecd5618e...", "section_id": 42, "token": "<api_token>"}'
```

---

### POST /have_rewards

Checks whether an actor has received mining rewards within a given time period.

**Body:**
```json
{
  "actor_id": "<actor id>",
  "period": "1d",
  "token": "<api token>"
}
```

**Period format:** `Xd Xh Xm Xs` (e.g. `1d`, `12h`, `30m`, `1d12h`). Defaults to `1d` if omitted.

**Response (200):**
```json
{
  "actor_id": "769cfeb282cd66f46cbb02d22e5e482e98ea92d8",
  "has_rewards": true,
  "reward_count": 3,
  "period_ms": 86400000,
  "period_str": "1d"
}
```

**Errors:**
| Code | Reason |
|------|--------|
| 400 | `actor_id required` / `invalid actor_id` / `token is not valid` |

**Example:**
```bash
curl -X POST http://<host>:17581/have_rewards \
  -H "Content-Type: application/json" \
  -d '{"actor_id": "769cfeb282cd66f46cbb02d22e5e482e98ea92d8", "period": "12h", "token": "<api_token>"}'
```

---

### POST /subscription_state

Checks whether an actor has an active Raccoon subscription.

**Body:**
```json
{
  "actor_id": "<actor id>",
  "token": "<api token>"
}
```

**Response (200):**
```json
{
  "actor_id": "769cfeb282cd66f46cbb02d22e5e482e98ea92d8",
  "active": true,
  "subscribed": false
}
```

**Fields:**
- `active` — subscription feature is active on the network
- `subscribed` — this actor has an active subscription

**Errors:**
| Code | Reason |
|------|--------|
| 400 | `actor_id required` / `invalid actor_id` / `token is not valid` |
| 400 | `can not find subscription` |

**Example:**
```bash
curl -X POST http://<host>:17581/subscription_state \
  -H "Content-Type: application/json" \
  -d '{"actor_id": "769cfeb282cd66f46cbb02d22e5e482e98ea92d8", "token": "<api_token>"}'
```
