# Action Plan for Full MQTT 3.1/3.1.1 Compliance

## Summary

This document outlines the current compliance status of the PubSubClient library with the MQTT 3.1 and 3.1.1 specifications. It highlights what is already supported, what is missing or incomplete, and provides a detailed action plan to achieve full compliance.

---

## What Works (Compliant)

- MQTT 3.1 and 3.1.1 protocol version selection
- Basic CONNECT, CONNACK, PUBLISH, PUBACK, SUBSCRIBE, SUBACK, UNSUBSCRIBE, UNSUBACK, PINGREQ, PINGRESP, DISCONNECT packet handling
- Clean session flag
- Will message (topic, QoS, retain, payload)
- Username/password authentication
- QoS 0 and QoS 1 (with PUBACK)
- Retained messages (basic support)
- Keepalive and socket timeout
- Buffer size configuration
- Basic topic subscription and publication

---

## What Needs Improvement (Not Fully Compliant)

- **QoS 2 (Exactly Once Delivery):** Only partial implementation; lacks full state management and retransmission logic for PUBREC, PUBREL, PUBCOMP handshake.
- **Session Persistence:** No support for persistent sessions (unacknowledged messages are not stored and resent after reconnect).
- **Advanced Wildcard Topic Matching:** No strict validation or advanced parsing for MQTT wildcards (+/#) in topic filters.
- **Error Handling:** Limited protocol error handling and reporting (e.g., malformed packets, protocol violations).
- **Large Message Handling:** Buffer limitations may prevent handling of large MQTT packets as allowed by the spec.
- **Subscription Options:** No support for subscription options introduced in later specs (notably MQTT 5.0, but some edge cases in 3.1.1).
- **Duplicate Message Detection:** No explicit handling of duplicate PUBLISH packets (DUP flag).

---

## Action Plan & TODO Table

| Area                        | Issue/Gap Description                                 | Spec Reference      | Priority | Action Item / Notes                       |
|-----------------------------|------------------------------------------------------|---------------------|----------|-------------------------------------------|
| QoS 2 Support               | Incomplete handshake, no message persistence         | 4.3, 4.4, 4.5      | High     | Implement full PUBREC/PUBREL/PUBCOMP flow |
| Session Persistence         | No storage/resend of unacknowledged messages         | 4.1, 4.2           | High     | Add session state and message queue       |
| Wildcard Topic Matching     | No strict/advanced wildcard validation               | 4.7.1, 4.7.2       | Medium   | Improve topic filter parsing              |
| Error Handling              | Limited protocol error detection/reporting           | 4.8, 4.9           | Medium   | Add error checks and state codes          |
| Large Message Handling      | Buffer size limits, no fragmentation                 | 2.2.2, 2.2.3       | Medium   | Support larger buffers or fragmentation   |
| Duplicate Message Detection | No handling of DUP flag in PUBLISH                   | 3.3.1, 4.3         | Low      | Track and filter duplicate messages       |
| Subscription Options        | No support for advanced subscribe options            | 3.8.1, 3.8.4       | Low      | Review and extend SUBSCRIBE handling      |
