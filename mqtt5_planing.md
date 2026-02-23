# MQTT 5 Implementation Plan for PubSubClient

> Incremental step-by-step plan to add MQTT 5.0 protocol support alongside the existing MQTT 3.1 / 3.1.1 support.
> Each step is self-contained and can be implemented, tested, and merged independently.

---

## Phase 1 — Foundation

These steps lay the groundwork. No MQTT 5 packets are exchanged yet, but the internal infrastructure is ready.

### Step 1: Version constant & runtime protocol selection

**Goal:** Allow the user to choose MQTT 5 at compile time or at runtime, while keeping the existing MQTT 3.1/3.1.1 default behavior unchanged.

**Changes:**

- Add `MQTT_VERSION_5 5` constant in `PubSubClient.h`.
- Add a `uint8_t _mqttVersion` member to `PubSubClient`, initialized to `MQTT_VERSION` (compile-time default).
- Add `PubSubClient& setMqttVersion(uint8_t version)` public method so users can switch protocol at runtime before `connect()`.
- Guard all MQTT-5-specific code paths with `if (_mqttVersion == MQTT_VERSION_5)`.

**Tests:** Unit test confirming version can be set/get and defaults to 3.1.1.

---

### Step 2: Variable Byte Integer encoding & decoding helpers

**Goal:** MQTT 5 uses Variable Byte Integer (VBI) encoding pervasively — for remaining length (already implemented), for property lengths, and inside properties themselves. Extract and expose reusable VBI encode/decode helpers.

**Changes:**

- Add a private `uint8_t encodeVariableByteInteger(uint32_t value, uint8_t* buf)` method that writes 1-4 bytes into `buf` and returns the number of bytes written.
- Add a private `uint32_t decodeVariableByteInteger(uint8_t* buf, uint8_t* bytesUsed)` method that reads from `buf` and returns the decoded value.
- Refactor `buildHeader()` and `readPacket()` to use these two helpers instead of inline VBI logic.

**Tests:** Encode/decode round-trip for edge values: 0, 127, 128, 16383, 16384, 268435455.

---

### Step 3: Property encoding & decoding framework

**Goal:** MQTT 5 packets carry a variable-length "properties" section. Build a lightweight, RAM-efficient framework to encode properties into the send buffer and decode them from the receive buffer.

**Changes:**

- Define all MQTT 5 property identifier constants (e.g., `MQTT_PROP_PAYLOAD_FORMAT 0x01`, `MQTT_PROP_MESSAGE_EXPIRY 0x02`, `MQTT_PROP_SESSION_EXPIRY 0x11`, `MQTT_PROP_RECEIVE_MAXIMUM 0x21`, `MQTT_PROP_TOPIC_ALIAS_MAX 0x22`, `MQTT_PROP_TOPIC_ALIAS 0x23`, etc.) in a new section of `PubSubClient.h`.
- Add a private `uint16_t writeProperty(uint8_t id, uint8_t* value, uint16_t len, uint8_t* buf, uint16_t pos)` helper that writes `[id][value]` into `buf` at `pos`.
- Add `uint16_t writePropertyU8/U16/U32/String/VBI(...)` convenience wrappers.
- Add a private `uint16_t skipProperties(uint8_t* buf, uint16_t pos)` helper that reads the property length VBI and advances `pos` past the entire properties section — used by code that doesn't need to parse a specific property.
- Add a private `bool findProperty(uint8_t* buf, uint16_t propsStart, uint16_t propsEnd, uint8_t id, uint8_t** valueOut, uint16_t* valueLenOut)` scanner that locates a specific property.

**Design note:** To minimise RAM, properties are always serialised directly into the existing `buffer[]` — no separate allocation.

**Tests:** Encode then decode a set of properties; verify `skipProperties()` returns the correct offset; verify `findProperty()` finds known and reports missing IDs.

---

### Step 4: CONNECT packet for MQTT 5

**Goal:** When `_mqttVersion == 5`, build and send a valid MQTT 5 CONNECT packet.

**Changes:**

- In `connect()`, when MQTT 5, write protocol name `MQTT` with protocol level byte `5`.
- After the existing variable header fields (connect flags, keep alive), write the **Connect Properties** section:
  - Property Length (VBI) — initially `0` (no properties), expandable in later steps.
- If Will is present, write **Will Properties** section (property length = 0 for now) before the Will Topic and Will Payload.
- Will Payload in MQTT 5 is length-prefixed (binary), unlike 3.1.1 where it is a UTF-8 string. Adjust encoding accordingly.
- Add a `boolean cleanStart` parameter alias — in MQTT 5 the flag is called "Clean Start" (same bit, different name). Keep backward compat with `cleanSession`.

**Tests:** Verify the CONNECT packet bytes on the wire match the MQTT 5 spec format: correct protocol level, properties length present, will properties present when will is set.

---

## Phase 2 — Core Packet Updates

Each packet type is updated to be MQTT-5-aware. Reason codes and property sections are handled.

### Step 5: CONNACK handling for MQTT 5

**Goal:** Parse the MQTT 5 CONNACK packet, which carries a Reason Code and a properties section (Session Present + Reason Code + Properties).

**Changes:**

- In the CONNACK parsing section of `connect()`, when MQTT 5:
  - Byte 1: remaining length (already read).
  - Byte 2: Connect Acknowledge Flags (Session Present flag — same as 3.1.1).
  - Byte 3: **Reason Code** (0x00 = Success, others = failure). Map to `_state`.
  - Remaining bytes: properties section → use `skipProperties()` initially; later steps will parse individual properties.
- Add new `MQTT_CONNECT_*` state constants for MQTT 5 specific reason codes (e.g., `0x97 Quota Exceeded`, `0x9C Use another server`).
- Store the **Server Keep Alive** property if present (overrides client's keepAlive).
- Store **Receive Maximum** from server (needed for flow control in Step 17).

**Tests:** Simulate broker CONNACK with reason code 0x00 (success), with properties; verify `state() == MQTT_CONNECTED`. Simulate failure codes; verify proper state.

---

### Step 6: PUBLISH for MQTT 5

**Goal:** Add a property section between the variable header and the payload in outgoing PUBLISH packets, and parse it in incoming PUBLISH packets.

**Changes:**

- **Outgoing:** After writing topic + msgId (for QoS > 0), insert the publish properties section (property length VBI + properties). Initially property length = 0.
  - Later steps will add Topic Alias, Message Expiry, etc.
- **Incoming:** In `loop()` PUBLISH handling, after reading topic + msgId, read and skip (or parse) the properties section before extracting the payload.
  - Adjust `payload` pointer and `payloadLength` calculation to account for the properties bytes.
- Add a new overloaded `publish()` that accepts properties (opaque buffer or builder pattern — TBD in implementation).

**Tests:** Outgoing QoS 0/1/2 PUBLISH packets have a properties section (length=0 for now). Incoming PUBLISH with properties is correctly parsed and the callback receives only the payload.

---

### Step 7: PUBACK / PUBREC / PUBREL / PUBCOMP with Reason Codes

**Goal:** In MQTT 5, all acknowledgment packets carry a Reason Code (1 byte) and an optional properties section.

**Changes:**

- **Outgoing (sendSimplePacket):** When MQTT 5, append Reason Code (`0x00` = Success) and Property Length = 0 after the msgId. Packet is now 5 bytes minimum instead of 4.
  - Optimization allowed by spec: if Reason Code is 0x00 and no properties, the entire Reason Code + Properties can be omitted (remaining length = 2). Implement this optimization to save bandwidth.
- **Incoming (loop):** When parsing PUBACK/PUBREC/PUBREL/PUBCOMP, read the optional Reason Code (if remaining length > 2) and skip properties.
  - On non-success Reason Code, treat as a protocol error (e.g., free the pending slot and optionally notify user).

**Tests:** Verify outgoing PUBACK/PUBREC/PUBREL/PUBCOMP are spec-compliant. Verify incoming with reason code 0x00 works. Verify non-zero reason code is handled gracefully.

---

### Step 8: SUBSCRIBE / UNSUBSCRIBE for MQTT 5

**Goal:** Add the properties section and new Subscription Options byte for MQTT 5 SUBSCRIBE. Add properties section to UNSUBSCRIBE.

**Changes:**

- **SUBSCRIBE (outgoing):** After msgId, insert properties section (length=0 initially). The Subscription Options byte (last byte per topic filter) in MQTT 5 has additional bits:
  - Bits 0-1: QoS (already used).
  - Bit 2: No Local (`0` by default).
  - Bit 3: Retain As Published (`0` by default).
  - Bits 4-5: Retain Handling (`0` by default).
  - Add optional parameters for these flags or a `subscribeOptions` struct.
- **UNSUBSCRIBE (outgoing):** After msgId, insert properties section (length=0).

**Tests:** Verify SUBSCRIBE packet on wire has correct format with properties and options byte. Verify UNSUBSCRIBE includes property section.

---

### Step 9: SUBACK / UNSUBACK handling for MQTT 5

**Goal:** Parse the MQTT 5 SUBACK and UNSUBACK, which carry per-topic Reason Codes and a properties section.

**Changes:**

- In `loop()`, add handling for MQTTSUBACK response:
  - Read properties section (skip initially).
  - Read Reason Code for each subscribed topic (one byte each). Reason Code >= 0x80 indicates failure.
  - Optionally expose subscription result to the user via a callback or return value.
- Similarly handle UNSUBACK with per-topic Reason Codes.

**Tests:** Simulate SUBACK with success and failure reason codes. Verify proper handling.

---

## Phase 3 — Important MQTT 5 Features

These steps add the features that distinguish MQTT 5 from 3.1.1.

### Step 10: Bidirectional DISCONNECT with Reason Code

**Goal:** In MQTT 5, both client and server can send DISCONNECT with a Reason Code and properties. The server may send DISCONNECT before closing the connection.

**Changes:**

- **Client disconnect():** When MQTT 5, send `DISCONNECT` with Reason Code (`0x00` = Normal) and Property Length = 0.
  - Add `disconnect(uint8_t reasonCode)` overload.
- **Server-initiated DISCONNECT:** In `loop()`, handle incoming `MQTTDISCONNECT` packet:
  - Read Reason Code and properties.
  - Set `_state` to an appropriate error code.
  - Close the connection.

**Tests:** Verify client DISCONNECT packet format. Simulate server DISCONNECT; verify state and connection closed.

---

### Step 11: AUTH packet support

**Goal:** MQTT 5 introduces the AUTH packet type (`0xF0`) for enhanced authentication (SASL-like challenge/response).

**Changes:**

- Add `MQTTAUTH (15 << 4)` packet type constant.
- In `connect()`, if an Authentication Method is set, include it in CONNECT properties.
- Handle incoming AUTH packets in `loop()`:
  - Read Reason Code + properties (Authentication Method, Authentication Data).
  - Invoke a user-provided auth callback to generate the response.
- Add `setAuthCallback(...)` method.
- Send AUTH packets with the response data.

**Design note:** This is the most complex new feature. Keep the interface minimal: one callback that receives auth data and returns response data.

**Tests:** Simulate a challenge/response flow; verify correct AUTH packets sent.

---

### Step 12: Session Expiry Interval

**Goal:** Allow the client to request a session expiry interval, and honour the server's value.

**Changes:**

- Add `setSessionExpiryInterval(uint32_t seconds)` method. `0` = session ends on disconnect (default), `0xFFFFFFFF` = session never expires.
- Write `MQTT_PROP_SESSION_EXPIRY` (`0x11`) into CONNECT properties.
- Parse Session Expiry Interval from CONNACK properties if present.
- On `disconnect()`, optionally include Session Expiry Interval property to override the value set at CONNECT.

**Tests:** Verify property appears in CONNECT. Verify server-provided value is parsed from CONNACK.

---

### Step 13: Topic Alias support

**Goal:** Topic Aliases map a topic string to a short integer, saving bandwidth on repeated publishes to the same topic.

**Changes:**

- Parse **Topic Alias Maximum** from CONNACK properties → store as `_serverTopicAliasMax`.
- Maintain a small alias table (fixed-size array, e.g., 4 entries): `{ uint16_t alias; const char* topic; }`.
- In `publish()`, if an alias exists for the topic, send the alias (2 bytes) with an empty topic string. If no alias exists and slots are available, assign one and send both topic + alias.
- In incoming PUBLISH parsing, if a Topic Alias property is present and topic string is empty, look up the alias in a receive-side table. If both are present, update the table.
- Add `setTopicAliasMaximum(uint16_t max)` to advertise client's own alias capacity in CONNECT properties.

**Design note:** Keep the table small (configurable via `MQTT_MAX_TOPIC_ALIASES`, default 4) to limit RAM use.

**Tests:** Publish with alias creation, then publish with alias reuse. Receive with alias. Verify alias maximum negotiation.

---

### Step 14: Message Expiry Interval

**Goal:** Allow outgoing messages to have a TTL, and expose incoming message's remaining TTL.

**Changes:**

- Add `publish()` overload or builder method that includes a `messageExpiry` (uint32_t seconds).
- Write `MQTT_PROP_MESSAGE_EXPIRY` (`0x02`) into PUBLISH properties when specified.
- When parsing incoming PUBLISH, if Message Expiry Interval property is present, pass it to the callback (or store it in an accessible field).

**Tests:** Verify property in outgoing PUBLISH. Verify parsing in incoming PUBLISH.

---

## Phase 4 — Advanced Features

### Step 15: User Properties

**Goal:** Allow arbitrary key-value string pairs in CONNECT, PUBLISH, SUBSCRIBE, UNSUBSCRIBE, DISCONNECT, and AUTH packets.

**Changes:**

- Add a lightweight builder: `addUserProperty(const char* key, const char* value)` that appends to a staging area in `buffer[]`.
- Write `MQTT_PROP_USER_PROPERTY` (`0x26`) entries into the properties section.
- Parse User Properties from incoming packets and expose via callback or accessor.

**Design note:** User properties can appear multiple times. Use a simple linear scan in the buffer for incoming, and a limited count (e.g., 4 max) for outgoing to limit memory usage.

**Tests:** Send and receive user properties.

---

### Step 16: Request-Response pattern

**Goal:** MQTT 5 formalizes request/response with Response Topic and Correlation Data properties.

**Changes:**

- Add `MQTT_PROP_RESPONSE_TOPIC` (`0x08`) and `MQTT_PROP_CORRELATION_DATA` (`0x09`).
- Add `publish()` overload or builder that accepts `responseTopic` and `correlationData`.
- In incoming PUBLISH, parse these properties and include them in the callback or expose via accessors.

**Tests:** Publish with response topic + correlation data. Receive and verify the properties are accessible.

---

### Step 17: Flow Control (Receive Maximum)

**Goal:** Honour the server's Receive Maximum (max in-flight QoS 1/2 messages) and advertise the client's own.

**Changes:**

- Parse `MQTT_PROP_RECEIVE_MAXIMUM` (`0x21`) from CONNACK → store as `_serverReceiveMax`. Default is 65535 per spec.
- Before sending a QoS 1/2 PUBLISH, check that the count of in-flight messages does not exceed `_serverReceiveMax`. If it does, block or return `false`.
- Write `MQTT_PROP_RECEIVE_MAXIMUM` in CONNECT properties with value = `MQTT_MAX_QOS_PENDING`.

**Tests:** Set server Receive Maximum to 2; attempt 3 QoS 1 publishes; verify the third is rejected until a PUBACK is received.

---

### Step 18: Maximum Packet Size & Server Reference

**Goal:** Respect the server's declared Maximum Packet Size and handle Server Reference (redirect).

**Changes:**

- Parse `MQTT_PROP_MAXIMUM_PACKET_SIZE` (`0x27`) from CONNACK.
- Before sending any packet that exceeds the server's max, return `false` or truncate.
- Parse `MQTT_PROP_SERVER_REFERENCE` (`0x1C`) from CONNACK and DISCONNECT.
- Add a method to retrieve the server reference string so the user can reconnect to the indicated server.

**Tests:** Verify oversized packet is rejected. Verify server reference is accessible after CONNACK/DISCONNECT.

---

### Step 19: Will Delay Interval & Payload Format

**Goal:** Support delayed will messages and the payload format indicator.

**Changes:**

- Add `setWillDelayInterval(uint32_t seconds)` → writes `MQTT_PROP_WILL_DELAY` (`0x18`) into Will Properties.
- Add `setPayloadFormatIndicator(bool utf8)` → writes `MQTT_PROP_PAYLOAD_FORMAT` (`0x01`) into PUBLISH or Will Properties.
- Parse these from incoming messages where relevant.

**Tests:** Verify Will Properties contain delay interval. Verify payload format indicator in PUBLISH.

---

### Step 20: Shared Subscriptions

**Goal:** Support the `$share/{ShareGroup}/{TopicFilter}` syntax.

**Changes:**

- No protocol-level changes needed — shared subscriptions use a special topic filter prefix.
- Add a helper or example: `subscribeShared(const char* group, const char* topicFilter)` that constructs the `$share/group/filter` string and calls `subscribe()`.
- Ensure that incoming messages on shared subscriptions are dispatched correctly.

**Tests:** Subscribe with `$share/` prefix; verify correct packets on wire.

---

## Phase 5 — Testing, Examples & Documentation

### Step 21: Comprehensive MQTT 5 test suite

**Goal:** Full test coverage for all MQTT 5 packet types and features.

**Changes:**

- Create `tests/src/mqtt5_connect_spec.cpp` — CONNECT/CONNACK with all property combinations.
- Create `tests/src/mqtt5_publish_spec.cpp` — PUBLISH with properties, topic alias, message expiry.
- Create `tests/src/mqtt5_subscribe_spec.cpp` — SUBSCRIBE/SUBACK with subscription options.
- Create `tests/src/mqtt5_disconnect_spec.cpp` — Bidirectional DISCONNECT.
- Create `tests/src/mqtt5_auth_spec.cpp` — AUTH challenge/response.
- Create `tests/src/mqtt5_flow_spec.cpp` — Flow control / Receive Maximum.
- Update `Makefile` to build and run all new test files.

---

### Step 22: Backward compatibility validation

**Goal:** Ensure MQTT 3.1 and 3.1.1 behavior is 100% preserved.

**Changes:**

- Run the full existing test suite with `MQTT_VERSION` set to 3.1 and 3.1.1.
- Add regression tests that explicitly verify MQTT 3.1.1 packets are unchanged when `_mqttVersion != 5`.
- Verify that the library compiles and works without any MQTT 5 code being activated (no binary size increase for 3.1.1-only users when using compile-time guards).

---

### Step 23: Examples & documentation

**Goal:** Provide ready-to-use examples for common MQTT 5 use cases.

**Changes:**

- `examples/mqtt5_basic/mqtt5_basic.ino` — Connect with MQTT 5, publish and subscribe.
- `examples/mqtt5_properties/mqtt5_properties.ino` — Publish with message expiry and user properties.
- `examples/mqtt5_auth/mqtt5_auth.ino` — Enhanced authentication flow.
- `examples/mqtt5_topic_alias/mqtt5_topic_alias.ino` — Topic alias usage for bandwidth savings.
- Update `README.md` with MQTT 5 usage instructions, API reference, and migration guide from 3.1.1.
- Add inline Doxygen-style comments to all new public methods.

---

### Step 24: Memory optimization & compile-time guards

**Goal:** Make MQTT 5 support optional at compile time so users who only need 3.1.1 pay no extra RAM or flash cost.

**Changes:**

- Add `MQTT_ENABLE_V5` compile-time flag (default: enabled). When disabled, all MQTT-5-specific code, constants, and data members are compiled out.
- Profile RAM and flash usage on ESP8266 and AVR with and without MQTT 5 enabled. Document the delta.
- Optimize property tables and alias tables to use minimal RAM (PROGMEM where possible on AVR).

---

## Implementation Order Summary

| Order | Step | Phase | Dependency |
| ----- | ---- | ----- | ---------- |
| 1 | Step 1 — Version constant & runtime selection | Foundation | None |
| 2 | Step 2 — VBI encode/decode helpers | Foundation | None |
| 3 | Step 3 — Property framework | Foundation | Step 2 |
| 4 | Step 4 — CONNECT for MQTT 5 | Foundation | Steps 1, 2, 3 |
| 5 | Step 5 — CONNACK handling | Core | Step 4 |
| 6 | Step 6 — PUBLISH for MQTT 5 | Core | Steps 3, 5 |
| 7 | Step 7 — ACK packets with Reason Codes | Core | Step 5 |
| 8 | Step 8 — SUBSCRIBE / UNSUBSCRIBE | Core | Steps 3, 5 |
| 9 | Step 9 — SUBACK / UNSUBACK | Core | Step 8 |
| 10 | Step 10 — Bidirectional DISCONNECT | Features | Step 5 |
| 11 | Step 11 — AUTH packet | Features | Steps 5, 3 |
| 12 | Step 12 — Session Expiry Interval | Features | Steps 4, 5 |
| 13 | Step 13 — Topic Alias | Features | Steps 5, 6 |
| 14 | Step 14 — Message Expiry | Features | Step 6 |
| 15 | Step 15 — User Properties | Advanced | Step 3 |
| 16 | Step 16 — Request-Response | Advanced | Step 6 |
| 17 | Step 17 — Flow Control | Advanced | Steps 5, 7 |
| 18 | Step 18 — Max Packet Size & Server Ref | Advanced | Step 5 |
| 19 | Step 19 — Will Delay & Payload Format | Advanced | Steps 4, 6 |
| 20 | Step 20 — Shared Subscriptions | Advanced | Step 8 |
| 21 | Step 21 — Test suite | Polish | All |
| 22 | Step 22 — Backward compat validation | Polish | All |
| 23 | Step 23 — Examples & docs | Polish | All |
| 24 | Step 24 — Memory optimization | Polish | All |

---

## Notes

- **Backward compatibility is non-negotiable.** Every change must keep MQTT 3.1/3.1.1 working identically.
- **RAM budget:** Target < 200 bytes additional RAM for MQTT 5 on AVR (beyond existing footprint).
- **Flash budget:** Target < 4 KB additional flash for full MQTT 5 support.
- **Each step should compile, pass all tests, and be independently mergeable.**
