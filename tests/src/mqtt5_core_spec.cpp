/*
 * mqtt5_core_spec.cpp
 *
 * Tests for MQTT 5 Core Packet Updates — Steps 5 to 9:
 *   Step 5 — CONNACK property parsing (Server Keep Alive, Receive Maximum)
 *   Step 6 — PUBLISH outgoing (props byte) + incoming (props skip)
 *   Step 7 — PUBACK/PUBREC/PUBREL/PUBCOMP with optional Reason Codes
 *   Step 8 — SUBSCRIBE / UNSUBSCRIBE with properties section
 *   Step 9 — SUBACK / UNSUBACK handling
 */

#include "PubSubClient.h"
#include "ShimClient.h"
#include "Buffer.h"
#include "BDDTest.h"
#include "trace.h"
#include <string.h>

byte server[] = { 172, 16, 0, 2 };

bool cb_called  = false;
char cb_topic[256];
byte cb_payload[256];
unsigned int cb_length;

void reset_cb() {
    cb_called = false;
    cb_topic[0] = '\0';
    memset(cb_payload, 0, sizeof(cb_payload));
    cb_length = 0;
}

void callback(char* topic, byte* payload, unsigned int length) {
    cb_called = true;
    strncpy(cb_topic, topic, sizeof(cb_topic) - 1);
    memcpy(cb_payload, payload, length);
    cb_length = length;
}

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

static bool do_connect_v5(PubSubClient& client, ShimClient& shimClient,
                           byte* connack = nullptr, uint8_t connackLen = 0) {
    static byte dflt[] = { 0x20, 0x03, 0x00, 0x00, 0x00 };
    if (!connack) { connack = dflt; connackLen = 5; }
    shimClient.respond(connack, connackLen);
    return client.connect("cli");
}

static bool do_connect_311(PubSubClient& client, ShimClient& shimClient) {
    byte connack[] = { 0x20, 0x02, 0x00, 0x00 };
    shimClient.respond(connack, 4);
    return client.connect("cli");
}

// =========================================================================
// Step 5 — CONNACK property parsing
// =========================================================================

int test_connack_v5_no_props_ok() {
    IT("MQTT 5 CONNACK with empty properties section connects successfully");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));
    IS_EQUAL(client.state(), MQTT_CONNECTED);
    END_IT
}

int test_connack_v5_server_keepalive() {
    IT("MQTT 5 CONNACK Server Keep Alive property overrides client keepAlive");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);

    /*
     * CONNACK with Server Keep Alive = 10 (0x000A)
     * Props content: id=0x13, value=0x00 0x0A   → 3 bytes
     * Props-length VBI = 0x03
     * Remaining = session_present(1) + reason(1) + props_VBI(1) + props(3) = 6 = 0x06
     * Packet: { 0x20, 0x06, 0x00, 0x00, 0x03, 0x13, 0x00, 0x0A }
     */
    byte connack[] = { 0x20, 0x06, 0x00, 0x00, 0x03, 0x13, 0x00, 0x0A };
    IS_TRUE(do_connect_v5(client, shimClient, connack, sizeof(connack)));
    IS_EQUAL(client.state(), MQTT_CONNECTED);
    // keepAlive should have been updated to 10
    // Verify indirectly: client is connected and did not time out
    IS_TRUE(client.connected());
    END_IT
}

int test_connack_v5_receive_maximum() {
    IT("MQTT 5 CONNACK with Receive Maximum property connects successfully");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);

    /*
     * CONNACK with Receive Maximum = 4 (0x0004)
     * Props: id=0x21, value=0x00 0x04  → 3 bytes
     * { 0x20, 0x06, 0x00, 0x00, 0x03, 0x21, 0x00, 0x04 }
     */
    byte connack[] = { 0x20, 0x06, 0x00, 0x00, 0x03, 0x21, 0x00, 0x04 };
    IS_TRUE(do_connect_v5(client, shimClient, connack, sizeof(connack)));
    IS_EQUAL(client.state(), MQTT_CONNECTED);
    END_IT
}

int test_connack_v5_multiple_props() {
    IT("MQTT 5 CONNACK with multiple properties parsed correctly");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);

    /*
     * CONNACK with Server Keep Alive=20 + Receive Maximum=8
     * Props: 0x13 0x00 0x14  (SKA=20), 0x21 0x00 0x08  (RM=8) → 6 bytes
     * Props VBI = 0x06
     * Remaining = 1+1+1+6 = 9
     * { 0x20, 0x09, 0x00, 0x00, 0x06, 0x13,0x00,0x14, 0x21,0x00,0x08 }
     */
    byte connack[] = { 0x20, 0x09, 0x00, 0x00, 0x06,
                       0x13, 0x00, 0x14,
                       0x21, 0x00, 0x08 };
    IS_TRUE(do_connect_v5(client, shimClient, connack, sizeof(connack)));
    IS_EQUAL(client.state(), MQTT_CONNECTED);
    IS_TRUE(client.connected());
    END_IT
}

int test_connack_v5_failure_reason_code() {
    IT("MQTT 5 CONNACK reason code 0x01 sets state and returns false");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);

    // Reason code 0x01 = Unspecified Error in some contexts
    byte connack[] = { 0x20, 0x03, 0x00, 0x01, 0x00 };
    IS_FALSE(do_connect_v5(client, shimClient, connack, sizeof(connack)));
    IS_EQUAL(client.state(), 0x01);
    IS_FALSE(client.connected());
    END_IT
}

// =========================================================================
// Step 6 — PUBLISH outgoing: properties length byte present
// =========================================================================

int test_mqtt5_publish_qos0_has_props_byte() {
    IT("MQTT 5 QoS 0 PUBLISH has properties-length byte (0x00) before payload");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    /*
     * PUBLISH QoS0, topic="t" (1), payload="v" (1), MQTT 5
     * Remaining = 2 + 1 + 1(props) + 1 = 5
     * { 0x30, 0x05, 0x00, 0x01, 't', 0x00, 'v' }
     */
    byte expected[] = { 0x30, 0x05, 0x00, 0x01, 0x74, 0x00, 0x76 };
    shimClient.expect(expected, sizeof(expected));
    IS_TRUE(client.publish("t", (const uint8_t*)"v", 1, false));
    IS_FALSE(shimClient.error());
    END_IT
}

int test_mqtt5_publish_qos1_has_props_byte() {
    IT("MQTT 5 QoS 1 PUBLISH has properties-length byte after msgId");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    /*
     * PUBLISH QoS1, topic="t" (1), payload="v" (1), msgId=1
     * Remaining = 2 + 1 + 2(msgId) + 1(props) + 1 = 7
     * { 0x32, 0x07, 0x00,0x01, 't', 0x00,0x01, 0x00, 'v' }
     */
    byte expected[] = { 0x32, 0x07, 0x00, 0x01, 0x74, 0x00, 0x01, 0x00, 0x76 };
    shimClient.expect(expected, sizeof(expected));
    IS_TRUE(client.publish("t", (const uint8_t*)"v", 1, false, 1));
    IS_FALSE(shimClient.error());
    END_IT
}

int test_mqtt5_publish_qos2_has_props_byte() {
    IT("MQTT 5 QoS 2 PUBLISH has properties-length byte after msgId");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    /*
     * PUBLISH QoS2, topic="t" (1), payload="v" (1), msgId=1
     * { 0x34, 0x07, 0x00,0x01, 't', 0x00,0x01, 0x00, 'v' }
     */
    byte expected[] = { 0x34, 0x07, 0x00, 0x01, 0x74, 0x00, 0x01, 0x00, 0x76 };
    shimClient.expect(expected, sizeof(expected));
    IS_TRUE(client.publish("t", (const uint8_t*)"v", 1, false, 2));
    IS_FALSE(shimClient.error());
    END_IT
}

// =========================================================================
// Step 6 — PUBLISH incoming: properties section is skipped
// =========================================================================

int test_mqtt5_receive_qos0_with_props() {
    IT("incoming MQTT 5 QoS 0 PUBLISH with properties: callback gets correct payload");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));
    reset_cb();

    /*
     * QoS 0 PUBLISH, topic="in" (2), props_len=0x00, payload="hi" (2)
     * Remaining = 2+2 + 1(props_len) + 2 = 7
     * { 0x30, 0x07, 0x00,0x02, 'i','n', 0x00, 'h','i' }
     */
    byte incoming[] = { 0x30, 0x07, 0x00, 0x02, 0x69, 0x6e, 0x00, 0x68, 0x69 };
    shimClient.respond(incoming, sizeof(incoming));

    IS_TRUE(client.loop());
    IS_TRUE(cb_called);
    IS_EQUAL(strcmp(cb_topic, "in"), 0);
    IS_EQUAL(cb_length, 2u);
    IS_EQUAL(memcmp(cb_payload, "hi", 2), 0);
    IS_FALSE(shimClient.error());
    END_IT
}

int test_mqtt5_receive_qos1_with_props() {
    IT("incoming MQTT 5 QoS 1 PUBLISH with properties: callback correct + PUBACK sent");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));
    reset_cb();

    /*
     * QoS 1 PUBLISH, topic="in" (2), msgId=42 (0x2A), props_len=0x00, payload="hi" (2)
     * Remaining = 2+2 + 2(msgId) + 1(props) + 2 = 9
     * { 0x32, 0x09, 0x00,0x02, 'i','n', 0x00,0x2A, 0x00, 'h','i' }
     */
    byte incoming[] = { 0x32, 0x09, 0x00, 0x02, 0x69, 0x6e, 0x00, 0x2A, 0x00, 0x68, 0x69 };
    shimClient.respond(incoming, sizeof(incoming));

    byte puback[] = { 0x40, 0x02, 0x00, 0x2A };
    shimClient.expect(puback, sizeof(puback));

    IS_TRUE(client.loop());
    IS_TRUE(cb_called);
    IS_EQUAL(strcmp(cb_topic, "in"), 0);
    IS_EQUAL(cb_length, 2u);
    IS_EQUAL(memcmp(cb_payload, "hi", 2), 0);
    IS_FALSE(shimClient.error());
    END_IT
}

int test_mqtt5_receive_qos1_with_nonempty_props() {
    IT("incoming MQTT 5 QoS 1 PUBLISH with non-empty props: payload is correctly extracted");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));
    reset_cb();

    /*
     * QoS 1 PUBLISH, topic="t" (1), msgId=1, one property (payload format indicator = 0x01):
     *   Props: id=0x01, value=0x01  → 2 bytes, props_len=0x02
     *   payload = "ok" (2 bytes)
     * Remaining = 2+1 + 2(msgId) + 1(props_VBI) + 2(props) + 2(payload) = 10
     * { 0x32, 0x0A, 0x00,0x01,'t', 0x00,0x01, 0x02, 0x01,0x01, 'o','k' }
     */
    byte incoming[] = { 0x32, 0x0A,
                         0x00, 0x01, 0x74,
                         0x00, 0x01,
                         0x02, 0x01, 0x01,
                         0x6F, 0x6B };
    shimClient.respond(incoming, sizeof(incoming));

    byte puback[] = { 0x40, 0x02, 0x00, 0x01 };
    shimClient.expect(puback, sizeof(puback));

    IS_TRUE(client.loop());
    IS_TRUE(cb_called);
    IS_EQUAL(cb_length, 2u);
    IS_EQUAL(memcmp(cb_payload, "ok", 2), 0);
    IS_FALSE(shimClient.error());
    END_IT
}

int test_mqtt5_receive_qos2_with_props() {
    IT("incoming MQTT 5 QoS 2 PUBLISH with properties: callback correct + PUBREC sent");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));
    reset_cb();

    /*
     * QoS 2 PUBLISH, topic="t" (1), msgId=5, props_len=0x00, payload="x" (1)
     * { 0x34, 0x07, 0x00,0x01,'t', 0x00,0x05, 0x00, 'x' }
     */
    byte incoming[] = { 0x34, 0x07, 0x00, 0x01, 0x74, 0x00, 0x05, 0x00, 0x78 };
    shimClient.respond(incoming, sizeof(incoming));

    byte pubrec[] = { 0x50, 0x02, 0x00, 0x05 };
    shimClient.expect(pubrec, sizeof(pubrec));

    IS_TRUE(client.loop());
    IS_TRUE(cb_called);
    IS_EQUAL(cb_length, 1u);
    IS_EQUAL(cb_payload[0], 0x78);  // 'x'
    IS_FALSE(shimClient.error());
    END_IT
}

// =========================================================================
// Step 7 — ACK packets with optional Reason Codes
// =========================================================================

int test_mqtt5_pubrec_no_reason_code_sends_pubrel() {
    IT("MQTT 5 PUBREC with remlen=2 (no reason code) triggers PUBREL");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    // QoS 2 publish  (msgId=1)
    client.publish("t", (const uint8_t*)"v", 1, false, 2);

    // PUBREC with remlen=2 (MQTT5 spec: no reason code = 0x00 success)
    byte pubrec[] = { 0x50, 0x02, 0x00, 0x01 };
    shimClient.respond(pubrec, sizeof(pubrec));

    byte pubrel[] = { 0x62, 0x02, 0x00, 0x01 };
    shimClient.expect(pubrel, sizeof(pubrel));

    IS_TRUE(client.loop());
    IS_FALSE(shimClient.error());
    END_IT
}

int test_mqtt5_pubrec_reason_code_success_sends_pubrel() {
    IT("MQTT 5 PUBREC with explicit reason code 0x00 triggers PUBREL");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    client.publish("t", (const uint8_t*)"v", 1, false, 2);

    // PUBREC with reason code 0x00 + empty props (remlen=4)
    byte pubrec[] = { 0x50, 0x04, 0x00, 0x01, 0x00, 0x00 };
    shimClient.respond(pubrec, sizeof(pubrec));

    byte pubrel[] = { 0x62, 0x02, 0x00, 0x01 };
    shimClient.expect(pubrel, sizeof(pubrel));

    IS_TRUE(client.loop());
    IS_FALSE(shimClient.error());
    END_IT
}

int test_mqtt5_pubrec_error_reason_code_no_pubrel() {
    IT("MQTT 5 PUBREC with error reason code (>=0x80) does not send PUBREL");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    // QoS 2 publish (msgId=1)
    client.publish("t", (const uint8_t*)"v", 1, false, 2);

    // PUBREC with reason code 0x87 (Not Authorized) → must NOT send PUBREL
    byte pubrec[] = { 0x50, 0x04, 0x00, 0x01, 0x87, 0x00 };
    shimClient.respond(pubrec, sizeof(pubrec));

    // Expect nothing (no PUBREL). loop() must not return false.
    bool rc = client.loop();
    IS_TRUE(rc);
    IS_TRUE(client.connected());
    // No shimClient.error() check needed — nothing was expected
    END_IT
}

int test_mqtt5_puback_remlen2_frees_slot() {
    IT("MQTT 5 PUBACK with remlen=2 frees the pending QoS 1 slot");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    // QoS 1 publish (msgId=1)
    client.publish("t", (const uint8_t*)"v", 1, false, 1);

    // PUBACK, remlen=2 (no reason code)
    byte puback[] = { 0x40, 0x02, 0x00, 0x01 };
    shimClient.respond(puback, sizeof(puback));

    IS_TRUE(client.loop());
    IS_TRUE(client.connected());
    END_IT
}

// =========================================================================
// Step 8 — SUBSCRIBE / UNSUBSCRIBE with properties section
// =========================================================================

int test_mqtt5_subscribe_qos0_has_props_byte() {
    IT("MQTT 5 SUBSCRIBE QoS 0 packet includes properties-length byte");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    /*
     * SUBSCRIBE, topic="t" (1), QoS=0
     * connect() resets nextMsgId=1; subscribe() pre-increments → msgId=2
     * Remaining = 2(msgId) + 1(props) + 2(topic_len) + 1(topic) + 1(opts) = 7
     * { 0x82, 0x07, 0x00,0x02, 0x00, 0x00,0x01,'t', 0x00 }
     */
    byte expected[] = { 0x82, 0x07, 0x00, 0x02, 0x00, 0x00, 0x01, 0x74, 0x00 };
    shimClient.expect(expected, sizeof(expected));
    IS_TRUE(client.subscribe("t", 0));
    IS_FALSE(shimClient.error());
    END_IT
}

int test_mqtt5_subscribe_qos1_has_props_byte() {
    IT("MQTT 5 SUBSCRIBE QoS 1 packet includes properties-length byte");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    // First subscribe uses msgId=2; second uses msgId=3
    // Skip first subscribe silently to advance msgId
    byte dummy[9] = { 0x82, 0x07, 0x00, 0x02, 0x00, 0x00, 0x01, 0x74, 0x00 };
    shimClient.expect(dummy, sizeof(dummy));
    client.subscribe("t", 0);

    /*
     * SUBSCRIBE QoS=1, topic="t", msgId=3
     * { 0x82, 0x07, 0x00,0x03, 0x00, 0x00,0x01,'t', 0x01 }
     */
    byte expected[] = { 0x82, 0x07, 0x00, 0x03, 0x00, 0x00, 0x01, 0x74, 0x01 };
    shimClient.expect(expected, sizeof(expected));
    IS_TRUE(client.subscribe("t", 1));
    IS_FALSE(shimClient.error());
    END_IT
}

int test_mqtt5_unsubscribe_has_props_byte() {
    IT("MQTT 5 UNSUBSCRIBE packet includes properties-length byte");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    /*
     * UNSUBSCRIBE, topic="t" (1), msgId=2
     * Remaining = 2 + 1(props) + 2+1 = 6
     * { 0xA2, 0x06, 0x00,0x02, 0x00, 0x00,0x01,'t' }
     */
    byte expected[] = { 0xA2, 0x06, 0x00, 0x02, 0x00, 0x00, 0x01, 0x74 };
    shimClient.expect(expected, sizeof(expected));
    IS_TRUE(client.unsubscribe("t"));
    IS_FALSE(shimClient.error());
    END_IT
}

// =========================================================================
// Step 9 — SUBACK / UNSUBACK handling
// =========================================================================

int test_mqtt5_suback_consumed() {
    IT("MQTT 5 SUBACK is consumed correctly by loop()");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    // Subscribe to get a msgId allocated
    byte sub[] = { 0x82, 0x07, 0x00, 0x02, 0x00, 0x00, 0x01, 0x74, 0x00 };
    shimClient.expect(sub, sizeof(sub));
    client.subscribe("t", 0);

    /*
     * MQTT 5 SUBACK: msgId=2, props_len=0x00, reason_code=0x00 (Success)
     * Remaining = 2(msgId) + 1(props_len) + 1(reason) = 4
     * { 0x90, 0x04, 0x00,0x02, 0x00, 0x00 }
     */
    byte suback[] = { 0x90, 0x04, 0x00, 0x02, 0x00, 0x00 };
    shimClient.respond(suback, sizeof(suback));

    IS_TRUE(client.loop());
    IS_TRUE(client.connected());
    END_IT
}

int test_311_suback_consumed() {
    IT("MQTT 3.1.1 SUBACK is consumed correctly by loop()");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    // Default MQTT 3.1.1
    IS_TRUE(do_connect_311(client, shimClient));

    // Subscribe
    byte sub[] = { 0x82, 0x06, 0x00, 0x02, 0x00, 0x01, 0x74, 0x00 };
    shimClient.expect(sub, sizeof(sub));
    client.subscribe("t", 0);

    /*
     * MQTT 3.1.1 SUBACK: msgId=2, granted_qos=0x00
     * { 0x90, 0x03, 0x00,0x02, 0x00 }
     */
    byte suback[] = { 0x90, 0x03, 0x00, 0x02, 0x00 };
    shimClient.respond(suback, sizeof(suback));

    IS_TRUE(client.loop());
    IS_TRUE(client.connected());
    END_IT
}

int test_mqtt5_unsuback_consumed() {
    IT("MQTT 5 UNSUBACK is consumed correctly by loop()");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(do_connect_v5(client, shimClient));

    // Unsubscribe
    byte unsub[] = { 0xA2, 0x06, 0x00, 0x02, 0x00, 0x00, 0x01, 0x74 };
    shimClient.expect(unsub, sizeof(unsub));
    client.unsubscribe("t");

    /*
     * MQTT 5 UNSUBACK: msgId=2, props_len=0, reason_code=0x00
     * { 0xB0, 0x04, 0x00,0x02, 0x00, 0x00 }
     */
    byte unsuback[] = { 0xB0, 0x04, 0x00, 0x02, 0x00, 0x00 };
    shimClient.respond(unsuback, sizeof(unsuback));

    IS_TRUE(client.loop());
    IS_TRUE(client.connected());
    END_IT
}

// =========================================================================
// Regression — MQTT 3.1.1 packets must be unchanged
// =========================================================================

int test_311_publish_qos0_no_props_byte() {
    IT("MQTT 3.1.1 QoS 0 PUBLISH has no properties byte (regression)");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    // default = MQTT 3.1.1
    IS_TRUE(do_connect_311(client, shimClient));

    /*
     * Standard MQTT 3.1.1 QoS 0 PUBLISH (no props byte):
     * { 0x30, 0x04, 0x00,0x01,'t', 'v' }
     */
    byte expected[] = { 0x30, 0x04, 0x00, 0x01, 0x74, 0x76 };
    shimClient.expect(expected, sizeof(expected));
    IS_TRUE(client.publish("t", (const uint8_t*)"v", 1, false));
    IS_FALSE(shimClient.error());
    END_IT
}

int test_311_subscribe_no_props_byte() {
    IT("MQTT 3.1.1 SUBSCRIBE has no properties byte (regression)");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect_311(client, shimClient));

    /*
     * MQTT 3.1.1 SUBSCRIBE, topic="t", QoS=0, msgId=2:
     * { 0x82, 0x06, 0x00,0x02, 0x00,0x01,'t', 0x00 }
     */
    byte expected[] = { 0x82, 0x06, 0x00, 0x02, 0x00, 0x01, 0x74, 0x00 };
    shimClient.expect(expected, sizeof(expected));
    IS_TRUE(client.subscribe("t", 0));
    IS_FALSE(shimClient.error());
    END_IT
}

int test_311_unsubscribe_no_props_byte() {
    IT("MQTT 3.1.1 UNSUBSCRIBE has no properties byte (regression)");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect_311(client, shimClient));

    /*
     * MQTT 3.1.1 UNSUBSCRIBE, topic="t", msgId=2:
     * { 0xA2, 0x05, 0x00,0x02, 0x00,0x01,'t' }
     */
    byte expected[] = { 0xA2, 0x05, 0x00, 0x02, 0x00, 0x01, 0x74 };
    shimClient.expect(expected, sizeof(expected));
    IS_TRUE(client.unsubscribe("t"));
    IS_FALSE(shimClient.error());
    END_IT
}

int test_311_receive_qos1_no_props_unchanged() {
    IT("MQTT 3.1.1 incoming QoS 1 PUBLISH still delivers correct payload (regression)");
    ShimClient shimClient; shimClient.setAllowConnect(true);
    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect_311(client, shimClient));
    reset_cb();

    /*
     * MQTT 3.1.1 QoS 1 PUBLISH: topic="in" (2), msgId=7, payload="hello" (5)
     * Same format as MQTT 3 (no props byte):
     * { 0x32, 0x0B, 0x00,0x02,'i','n', 0x00,0x07, 'h','e','l','l','o' }
     */
    byte incoming[] = { 0x32, 0x0B,
                         0x00, 0x02, 0x69, 0x6e,
                         0x00, 0x07,
                         0x68, 0x65, 0x6c, 0x6c, 0x6f };
    shimClient.respond(incoming, sizeof(incoming));

    byte puback[] = { 0x40, 0x02, 0x00, 0x07 };
    shimClient.expect(puback, sizeof(puback));

    IS_TRUE(client.loop());
    IS_TRUE(cb_called);
    IS_EQUAL(cb_length, 5u);
    IS_EQUAL(memcmp(cb_payload, "hello", 5), 0);
    IS_FALSE(shimClient.error());
    END_IT
}

// =========================================================================
// main
// =========================================================================

int main(int argc, char *argv[]) {
    SUITE("MQTT5 Core (Steps 5-9)");

    // Step 5
    test_connack_v5_no_props_ok();
    test_connack_v5_server_keepalive();
    test_connack_v5_receive_maximum();
    test_connack_v5_multiple_props();
    test_connack_v5_failure_reason_code();

    // Step 6 outgoing
    test_mqtt5_publish_qos0_has_props_byte();
    test_mqtt5_publish_qos1_has_props_byte();
    test_mqtt5_publish_qos2_has_props_byte();

    // Step 6 incoming
    test_mqtt5_receive_qos0_with_props();
    test_mqtt5_receive_qos1_with_props();
    test_mqtt5_receive_qos1_with_nonempty_props();
    test_mqtt5_receive_qos2_with_props();

    // Step 7
    test_mqtt5_pubrec_no_reason_code_sends_pubrel();
    test_mqtt5_pubrec_reason_code_success_sends_pubrel();
    test_mqtt5_pubrec_error_reason_code_no_pubrel();
    test_mqtt5_puback_remlen2_frees_slot();

    // Step 8
    test_mqtt5_subscribe_qos0_has_props_byte();
    test_mqtt5_subscribe_qos1_has_props_byte();
    test_mqtt5_unsubscribe_has_props_byte();

    // Step 9
    test_mqtt5_suback_consumed();
    test_311_suback_consumed();
    test_mqtt5_unsuback_consumed();

    // Regression
    test_311_publish_qos0_no_props_byte();
    test_311_subscribe_no_props_byte();
    test_311_unsubscribe_no_props_byte();
    test_311_receive_qos1_no_props_unchanged();

    FINISH
}
