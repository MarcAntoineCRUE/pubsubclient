/**
 * @file mqtt5_foundation_spec.cpp
 * @brief Tests for MQTT 5 Foundation (Steps 1, 2, 3):
 *   - Step 1: Version constant & runtime protocol selection
 *   - Step 2: Variable Byte Integer encode/decode helpers
 *   - Step 3: Property encoding/decoding framework (indirect tests)
 */
#include "BDDTest.h"
#include "Buffer.h"
#include "PubSubClient.h"
#include "ShimClient.h"
#include "trace.h"

byte server[] = {172, 16, 0, 2};

// function declarations
void callback(char* topic, uint8_t* payload, size_t plength);
int test_default_version_is_compile_time_default();
int test_set_version_5();
int test_set_version_3_1_1();
int test_set_version_returns_client();
int test_version_5_constant_value();
int test_vbi_1byte_remaining_length();
int test_vbi_2byte_remaining_length();
int test_vbi_decode_2byte_remaining_length();
int test_mqtt5_connack_with_properties_skipped();

void callback(_UNUSED_ char* topic, _UNUSED_ uint8_t* payload, _UNUSED_ size_t plength) {}

// ──────────────────────────────────────────────────────────────────────────────
// Step 1 — Version constant & runtime selection
// ──────────────────────────────────────────────────────────────────────────────

int test_default_version_is_compile_time_default() {
    IT("defaults to the compile-time MQTT_VERSION constant");
    ShimClient shimClient;
    PubSubClient client(shimClient);
    IS_EQUAL(client.getMqttVersion(), (uint8_t)MQTT_VERSION);
    END_IT
}

int test_set_version_5() {
    IT("can be set to MQTT 5");
    ShimClient shimClient;
    PubSubClient client(shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    IS_EQUAL(client.getMqttVersion(), (uint8_t)MQTT_VERSION_5);
    END_IT
}

int test_set_version_3_1_1() {
    IT("can be set to MQTT 3.1.1");
    ShimClient shimClient;
    PubSubClient client(shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    client.setMqttVersion(MQTT_VERSION_3_1_1);
    IS_EQUAL(client.getMqttVersion(), (uint8_t)MQTT_VERSION_3_1_1);
    END_IT
}

int test_set_version_returns_client() {
    IT("setMqttVersion returns the client (chaining)");
    ShimClient shimClient;
    PubSubClient client(shimClient);
    PubSubClient& ref = client.setMqttVersion(MQTT_VERSION_5);
    IS_TRUE(&ref == &client);
    END_IT
}

int test_version_5_constant_value() {
    IT("MQTT_VERSION_5 constant equals 5");
    IS_EQUAL((uint8_t)MQTT_VERSION_5, (uint8_t)5);
    END_IT
}

// ──────────────────────────────────────────────────────────────────────────────
// Step 2 — VBI encode/decode helpers (tested via observable packet bytes)
//
// The remaining length field is a Variable Byte Integer.  We verify correct
// encoding by checking the raw bytes of a subscribe packet.
// ──────────────────────────────────────────────────────────────────────────────

/**
 * VBI value 14 (fits in 1 byte).  A 3.1.1 CONNECT for "client_test1" has
 * remaining_length = 24 (0x18) — 1 byte.
 */
int test_vbi_1byte_remaining_length() {
    IT("VBI: remaining_length <= 127 is encoded in 1 byte");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    // CONNECT 3.1.1, id="client_test1", cleanSession, keepalive=15
    // remaining length = 24 = 0x18 → 1-byte VBI
    byte connect_pkt[] = {
        0x10, 0x18,                                  // header + 1-byte remaining length
        0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x04,  // protocol name + level 4
        0x02, 0x00, 0x0F,                             // flags + keepalive
        0x00, 0x0C,                                   // client id length
        0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x5F, 0x74, 0x65, 0x73, 0x74, 0x31  // "client_test1"
    };
    byte connack[] = {0x20, 0x02, 0x00, 0x00};
    shimClient.expect(connect_pkt, 26);
    shimClient.respond(connack, 4);

    PubSubClient client(server, 1883, callback, shimClient);
    bool rc = client.connect("client_test1");
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());
    END_IT
}

/**
 * VBI value 128 (requires 2 bytes: 0x80 0x01).
 * A SUBSCRIBE with msgId=1, a topic of 123 bytes ('a' * 123) and QoS=0
 * has remaining_length = 2 + 2 + 123 + 1 = 128.
 */
int test_vbi_2byte_remaining_length() {
    IT("VBI: remaining_length 128 is encoded in 2 bytes (0x80 0x01)");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    // Minimal connect / connack exchange
    byte connack[] = {0x20, 0x02, 0x00, 0x00};
    shimClient.respond(connack, 4);

    PubSubClient client(server, 1883, callback, shimClient);
    client.setBufferSize(512);
    client.connect("vbi_test");

    // Build expected SUBSCRIBE packet: remaining_length = 128 → 0x80 0x01
    // SUBSCRIBE header = 0x82 (type 8, QoS1 fixed flag set)
    // Packet = {0x82, 0x80, 0x01,  msgId(2),  topicLen(2),  topic(123),  QoS(1)}
    static byte subscribe_pkt[131];  // 1 + 2 + 2 + 2 + 123 + 1 = 131
    subscribe_pkt[0] = 0x82;  // SUBSCRIBE | QoS1
    subscribe_pkt[1] = 0x80;  // VBI byte 1
    subscribe_pkt[2] = 0x01;  // VBI byte 2  (= 128)
    subscribe_pkt[3] = 0x00;  // msgId MSB
    subscribe_pkt[4] = 0x02;  // msgId LSB (=2: connect() sets _nextMsgId=1, subscribe increments to 2)
    subscribe_pkt[5] = 0x00;  // topic length MSB
    subscribe_pkt[6] = 0x7B;  // topic length LSB (123)
    for (int i = 0; i < 123; i++) subscribe_pkt[7 + i] = 'a';
    subscribe_pkt[130] = 0x00;  // QoS 0
    shimClient.expect(subscribe_pkt, 131);

    // Build 123-char topic string
    char topic[124];
    for (int i = 0; i < 123; i++) topic[i] = 'a';
    topic[123] = '\0';

    bool rc = client.subscribe(topic);
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());
    END_IT
}

/**
 * VBI decode: receive an incoming PUBLISH with 2-byte remaining_length (>= 128).
 * A PUBLISH QoS 0 on topic "t" with 125-byte payload has remaining_length = 2+1+125 = 128.
 */
int test_vbi_decode_2byte_remaining_length() {
    IT("VBI: correctly decodes a 2-byte remaining_length in an incoming packet");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    byte connack[] = {0x20, 0x02, 0x00, 0x00};
    shimClient.respond(connack, 4);

    bool callbackCalled = false;
    size_t receivedLen = 0;

    PubSubClient client(server, 1883,
        [&](char* /*topic*/, uint8_t* /*payload*/, size_t plength) {
            callbackCalled = true;
            receivedLen = plength;
        },
        shimClient);

    client.setBufferSize(512);
    client.connect("vbi_decode_test");

    // Build PUBLISH with topic "t" (1 char) and 125 zero-bytes payload
    // remaining_length = 2 (topic_len) + 1 (topic) + 125 (payload) = 128 → 0x80 0x01
    static byte publish_pkt[3 + 2 + 1 + 125];  // hdr + rem_len(2) + topic_len(2) + topic(1) + payload(125)
    publish_pkt[0] = 0x30;  // PUBLISH, QoS 0, no retain
    publish_pkt[1] = 0x80;  // VBI byte 1
    publish_pkt[2] = 0x01;  // VBI byte 2 (= 128)
    publish_pkt[3] = 0x00;  // topic length MSB
    publish_pkt[4] = 0x01;  // topic length LSB
    publish_pkt[5] = 0x74;  // topic = 't'
    for (int i = 0; i < 125; i++) publish_pkt[6 + i] = 0xAB;
    shimClient.respond(publish_pkt, (int)sizeof(publish_pkt));

    client.loop();

    IS_TRUE(callbackCalled);
    IS_EQUAL(receivedLen, (size_t)125);
    END_IT
}

// ──────────────────────────────────────────────────────────────────────────────
// Step 3 — Property framework (indirect: CONNACK with properties is handled)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * skipProperties: a MQTT 5 CONNACK with non-empty properties section must be
 * parsed successfully and result in MQTT_CONNECTED state.
 */
int test_mqtt5_connack_with_properties_skipped() {
    IT("skipProperties: CONNACK with properties section is parsed correctly");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);
    byte expectServer[] = {172, 16, 0, 2};
    shimClient.expectConnect(expectServer, 1883);

    // MQTT5 basic connect (id="t", cleanStart, keepalive=15)
    // Will send an MQTT5 CONNECT; just let anything through for this test
    shimClient.respond(nullptr, 0);  // dummy, replaced by full CONNACK below

    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);

    // MQTT5 CONNACK with Receive Maximum (0x21, value=100) and Topic Alias Max (0x22, value=5)
    // Properties: {0x21, 0x00, 0x64,  0x22, 0x00, 0x05} → 6 bytes
    // Remaining length = 1 (sess) + 1 (reason) + 1 (props_len VBI) + 6 (props) = 9 = 0x09
    byte connack5[] = {
        0x20, 0x09,              // CONNACK header, remaining length = 9
        0x00,                    // Session Present = 0
        0x00,                    // Reason Code = 0x00 (Success)
        0x06,                    // Properties length = 6
        0x21, 0x00, 0x64,       // Receive Maximum = 100
        0x22, 0x00, 0x05        // Topic Alias Maximum = 5
    };
    // Re-create client so we can set a clean respond
    ShimClient shimClient2;
    shimClient2.setAllowConnect(true);
    shimClient2.respond(connack5, 11);

    PubSubClient client2(server, 1883, callback, shimClient2);
    client2.setMqttVersion(MQTT_VERSION_5);

    bool rc = client2.connect("t");
    IS_TRUE(rc);
    IS_FALSE(shimClient2.error());
    IS_EQUAL(client2.state(), (int)MQTT_CONNECTED);
    END_IT
}

// ──────────────────────────────────────────────────────────────────────────────
// main
// ──────────────────────────────────────────────────────────────────────────────

int main() {
    SUITE("MQTT 5 Foundation");

    // Step 1 — Version constant & runtime selection
    test_default_version_is_compile_time_default();
    test_set_version_5();
    test_set_version_3_1_1();
    test_set_version_returns_client();
    test_version_5_constant_value();

    // Step 2 — VBI encode/decode helpers
    test_vbi_1byte_remaining_length();
    test_vbi_2byte_remaining_length();
    test_vbi_decode_2byte_remaining_length();

    // Step 3 — Property framework (indirect / skipProperties)
    test_mqtt5_connack_with_properties_skipped();

    FINISH
}
