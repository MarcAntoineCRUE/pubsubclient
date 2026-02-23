/*
 * improvements_spec.cpp
 *
 * Tests pour les améliorations de la librairie PubSubClient :
 *  - QoS 1 publish (packet format + PUBACK handling)
 *  - QoS 2 publish (full PUBREC→PUBREL→PUBCOMP handshake)
 *  - QoS 1 receive (PUBLISH → PUBACK)
 *  - QoS 2 receive (PUBLISH → PUBREC → PUBREL → PUBCOMP)
 *  - Robustesse : QoS invalide, buffer trop petit, willQos invalide
 *  - Vérification des valeurs des macros de paquets
 *  - Vérification de l'état initial (init())
 */

#include "PubSubClient.h"
#include "ShimClient.h"
#include "Buffer.h"
#include "BDDTest.h"
#include "trace.h"
#include <string.h>

byte server[] = { 172, 16, 0, 2 };

bool cb_called   = false;
char cb_topic[256];
char cb_payload[256];
unsigned int cb_length;

void reset_cb() {
    cb_called  = false;
    cb_topic[0]   = '\0';
    cb_payload[0] = '\0';
    cb_length  = 0;
}

void callback(char* topic, byte* payload, unsigned int length) {
    cb_called = true;
    strncpy(cb_topic, topic, sizeof(cb_topic)-1);
    memcpy(cb_payload, payload, length);
    cb_length = length;
}

// -------------------------------------------------------------------------
// Helper: connect a client to a shimClient that has a connack queued up.
// -------------------------------------------------------------------------
static bool do_connect(PubSubClient &client, ShimClient &shimClient) {
    byte connack[] = { 0x20, 0x02, 0x00, 0x00 };
    shimClient.respond(connack, 4);
    return client.connect("test_client");
}

// =========================================================================
// 1. Macro values
// =========================================================================

int test_macro_values() {
    IT("has correct packet-type macro values");

    // Core defines that are used in protocol logic
    IS_EQUAL(MQTTCONNECT,       0x10);
    IS_EQUAL(MQTTPUBLISH,       0x30);
    IS_EQUAL(MQTTPUBACK,        0x40);
    IS_EQUAL(MQTTPUBREC,        0x50);
    IS_EQUAL(MQTTPUBREL,        0x60);
    IS_EQUAL(MQTTPUBCOMP,       0x70);
    IS_EQUAL(MQTTSUBSCRIBE,     0x80);
    IS_EQUAL(MQTTDISCONNECT,    0xE0);

    // QoS flags
    IS_EQUAL(MQTTQOS0,  0x00);
    IS_EQUAL(MQTTQOS1,  0x02);
    IS_EQUAL(MQTTQOS2,  0x04);

    // PUBREL with QoS-1 flag (must be 0x62 per MQTT spec §2.1.3)
    IS_EQUAL((MQTTPUBREL | MQTTQOS1), 0x62);

    END_IT
}

// =========================================================================
// 2. Initial state after construction
// =========================================================================

int test_initial_state() {
    IT("starts in DISCONNECTED state after construction");

    ShimClient shimClient;
    PubSubClient client(server, 1883, callback, shimClient);

    IS_EQUAL(client.state(),  MQTT_DISCONNECTED);
    IS_FALSE(client.connected());

    END_IT
}

// =========================================================================
// 3. QoS 1 publish — packet format
// =========================================================================

int test_qos1_publish_packet_format() {
    IT("QoS 1 publish sends correct header and msgId");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));

    /*
     * Expected PUBLISH QoS1 packet for topic="t" (1 byte), payload="v" (1 byte), msgId=1 :
     *  Fixed header : 0x32  (PUBLISH | QoS1)
     *  Rem. length  : 0x06  (2 topic-len + 1 topic + 2 msgId + 1 payload)
     *  Topic len    : 0x00, 0x01
     *  Topic        : 0x74  ('t')
     *  MsgId        : 0x00, 0x01
     *  Payload      : 0x76  ('v')
     */
    byte expected[] = { 0x32, 0x06, 0x00, 0x01, 0x74, 0x00, 0x01, 0x76 };
    shimClient.expect(expected, sizeof(expected));

    bool rc = client.publish("t", (const uint8_t*)"v", 1, false, 1);
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());

    END_IT
}

// =========================================================================
// 4. QoS 2 publish — packet format
// =========================================================================

int test_qos2_publish_packet_format() {
    IT("QoS 2 publish sends correct header and msgId");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));

    /*
     * Expected PUBLISH QoS2 packet for topic="t", payload="v", msgId=1 :
     *  Fixed header : 0x34  (PUBLISH | QoS2)
     *  Rem. length  : 0x06
     *  Topic len    : 0x00, 0x01
     *  Topic        : 0x74
     *  MsgId        : 0x00, 0x01
     *  Payload      : 0x76
     */
    byte expected[] = { 0x34, 0x06, 0x00, 0x01, 0x74, 0x00, 0x01, 0x76 };
    shimClient.expect(expected, sizeof(expected));

    bool rc = client.publish("t", (const uint8_t*)"v", 1, false, 2);
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());

    END_IT
}

// =========================================================================
// 5. QoS 2 outgoing — PUBREC → PUBREL handshake
// =========================================================================

int test_qos2_outgoing_pubrec_pubrel() {
    IT("QoS 2 outgoing: loop() sends PUBREL on PUBREC");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));

    // Publish QoS 2 (msgId=1)
    client.publish("t", (const uint8_t*)"v", 1, false, 2);

    // Broker responds with PUBREC for msgId=1
    byte pubrec[] = { 0x50, 0x02, 0x00, 0x01 };
    shimClient.respond(pubrec, sizeof(pubrec));

    // loop() must send PUBREL {0x62, 0x02, 0x00, 0x01}
    byte pubrel[] = { 0x62, 0x02, 0x00, 0x01 };
    shimClient.expect(pubrel, sizeof(pubrel));

    bool rc = client.loop();
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());

    END_IT
}

// =========================================================================
// 6. QoS 2 outgoing — PUBCOMP received after PUBREL
// =========================================================================

int test_qos2_outgoing_pubcomp() {
    IT("QoS 2 outgoing: loop() processes PUBCOMP without error");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));

    // Publish QoS 2 (msgId=1)
    client.publish("t", (const uint8_t*)"v", 1, false, 2);

    // PUBREC → client sends PUBREL
    byte pubrec[] = { 0x50, 0x02, 0x00, 0x01 };
    shimClient.respond(pubrec, sizeof(pubrec));
    client.loop(); // sends PUBREL

    // Broker now sends PUBCOMP
    byte pubcomp[] = { 0x70, 0x02, 0x00, 0x01 };
    shimClient.respond(pubcomp, sizeof(pubcomp));

    bool rc = client.loop(); // should process PUBCOMP cleanly
    IS_TRUE(rc);
    IS_TRUE(client.connected());
    IS_FALSE(shimClient.error());

    END_IT
}

// =========================================================================
// 7. QoS 1 receive — incoming PUBLISH triggers PUBACK
// =========================================================================

int test_receive_qos1_sends_puback() {
    IT("incoming QoS 1 PUBLISH triggers callback and PUBACK");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));
    reset_cb();

    /*
     * Incoming PUBLISH QoS1: topic="in" (2), payload="hello" (5), msgId=42
     *  0x32, 0x0B,  0x00,0x02, 'i','n',  0x00,0x2A,  'h','e','l','l','o'
     */
    byte incoming[] = {
        0x32, 0x0B,
        0x00, 0x02, 0x69, 0x6e,
        0x00, 0x2A,
        0x68, 0x65, 0x6c, 0x6c, 0x6f
    };
    shimClient.respond(incoming, sizeof(incoming));

    // Expect PUBACK for msgId=42
    byte puback[] = { 0x40, 0x02, 0x00, 0x2A };
    shimClient.expect(puback, sizeof(puback));

    bool rc = client.loop();
    IS_TRUE(rc);
    IS_TRUE(cb_called);
    IS_EQUAL(strcmp(cb_topic, "in"), 0);
    IS_EQUAL(cb_length, 5u);
    IS_EQUAL(memcmp(cb_payload, "hello", 5), 0);
    IS_FALSE(shimClient.error());

    END_IT
}

// =========================================================================
// 8. QoS 2 receive — incoming PUBLISH triggers PUBREC
// =========================================================================

int test_receive_qos2_sends_pubrec() {
    IT("incoming QoS 2 PUBLISH triggers callback and PUBREC");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));
    reset_cb();

    /*
     * Incoming PUBLISH QoS2: topic="in" (2), payload="hi" (2), msgId=99
     *  0x34, 0x08,  0x00,0x02, 'i','n',  0x00,0x63,  'h','i'
     */
    byte incoming[] = {
        0x34, 0x08,
        0x00, 0x02, 0x69, 0x6e,
        0x00, 0x63,
        0x68, 0x69
    };
    shimClient.respond(incoming, sizeof(incoming));

    // Expect PUBREC for msgId=99
    byte pubrec[] = { 0x50, 0x02, 0x00, 0x63 };
    shimClient.expect(pubrec, sizeof(pubrec));

    bool rc = client.loop();
    IS_TRUE(rc);
    IS_TRUE(cb_called);             // callback called on first delivery
    IS_EQUAL(strcmp(cb_topic, "in"), 0);
    IS_EQUAL(cb_length, 2u);
    IS_FALSE(shimClient.error());

    END_IT
}

// =========================================================================
// 9. QoS 2 receive — incoming PUBREL triggers PUBCOMP
// =========================================================================

int test_receive_qos2_full_handshake() {
    IT("QoS 2 receive full handshake: PUBLISH→PUBREC→PUBREL→PUBCOMP");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));
    reset_cb();

    // Step 1: broker sends PUBLISH QoS 2 (msgId=99)
    byte incoming[] = {
        0x34, 0x08,
        0x00, 0x02, 0x69, 0x6e,
        0x00, 0x63,
        0x68, 0x69
    };
    shimClient.respond(incoming, sizeof(incoming));
    // Expect PUBREC
    byte pubrec[] = { 0x50, 0x02, 0x00, 0x63 };
    shimClient.expect(pubrec, sizeof(pubrec));

    bool rc = client.loop(); // receives PUBLISH, sends PUBREC
    IS_TRUE(rc);
    IS_TRUE(cb_called);

    // Step 2: broker sends PUBREL (msgId=99)
    byte pubrel[] = { 0x62, 0x02, 0x00, 0x63 };
    shimClient.respond(pubrel, sizeof(pubrel));
    // Expect PUBCOMP
    byte pubcomp[] = { 0x70, 0x02, 0x00, 0x63 };
    shimClient.expect(pubcomp, sizeof(pubcomp));

    rc = client.loop(); // receives PUBREL, sends PUBCOMP
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());

    END_IT
}

// =========================================================================
// 10. QoS 2 duplicate suppression — duplicate PUBLISH not re-delivered
// =========================================================================

int test_receive_qos2_duplicate_not_redelivered() {
    IT("QoS 2 duplicate PUBLISH is not re-delivered to callback");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));
    reset_cb();

    // First delivery
    byte incoming[] = {
        0x34, 0x08,
        0x00, 0x02, 0x69, 0x6e,
        0x00, 0x63,
        0x68, 0x69
    };
    shimClient.respond(incoming, sizeof(incoming));
    byte pubrec[] = { 0x50, 0x02, 0x00, 0x63 };
    shimClient.expect(pubrec, sizeof(pubrec));
    client.loop();
    IS_TRUE(cb_called);

    // Duplicate delivery (same msgId=99, DUP flag=0x08 set in header)
    reset_cb();
    byte dup[] = {
        0x3C, 0x08,   // DUP flag set (0x34 | 0x08)
        0x00, 0x02, 0x69, 0x6e,
        0x00, 0x63,
        0x68, 0x69
    };
    shimClient.respond(dup, sizeof(dup));
    // Should still send PUBREC (slot already taken, same msgId)
    shimClient.expect(pubrec, sizeof(pubrec));
    client.loop();
    // Callback must NOT be called again
    IS_FALSE(cb_called);
    IS_FALSE(shimClient.error());

    END_IT
}

// =========================================================================
// 11. Invalid QoS (3) is rejected
// =========================================================================

int test_publish_invalid_qos_rejected() {
    IT("publish with QoS 3 is rejected and returns false");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));

    bool rc = client.publish("t", (const uint8_t*)"v", 1, false, 3);
    IS_FALSE(rc);

    END_IT
}

// =========================================================================
// 12. Oversized publish is rejected (buffer protection)
// =========================================================================

int test_publish_oversized_rejected() {
    IT("publish that would overflow the buffer returns false");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));  // connect with default 256-byte buffer

    // Shrink buffer AFTER connect — too small for a 30-byte payload
    IS_TRUE(client.setBufferSize(20));

    // Topic="t" (1) + payload (30) + header overhead > 20 bytes
    byte bigPayload[30];
    memset(bigPayload, 'A', sizeof(bigPayload));
    bool rc = client.publish("t", bigPayload, sizeof(bigPayload), false);
    IS_FALSE(rc);

    END_IT
}

// =========================================================================
// 13. connect() rejects willQos > 2
// =========================================================================

int test_connect_invalid_will_qos_rejected() {
    IT("connect with willQos=3 returns false without touching the socket");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    // Queue a connack just in case — it must NOT be consumed
    byte connack[] = { 0x20, 0x02, 0x00, 0x00 };
    shimClient.respond(connack, 4);

    bool rc = client.connect("id", NULL, NULL,
                             "will/topic", /*willQos=*/3, false, "msg", true);
    IS_FALSE(rc);
    IS_EQUAL(client.state(), MQTT_DISCONNECTED);

    END_IT
}

// =========================================================================
// 14. setBufferSize safety: shrink failure keeps old size
// =========================================================================

int test_setbuffersize_preserves_old_on_failure() {
    IT("setBufferSize(0) returns false and leaves buffer intact");

    ShimClient shimClient;
    PubSubClient client(server, 1883, callback, shimClient);

    uint16_t original = client.getBufferSize();
    bool rc = client.setBufferSize(0);
    IS_FALSE(rc);
    IS_EQUAL(client.getBufferSize(), original);

    END_IT
}

// =========================================================================
// 15. subscribe() accepts QoS 0, 1 and 2; rejects 3
// =========================================================================

int test_subscribe_qos_validation() {
    IT("subscribe accepts QoS 0/1/2 and rejects QoS 3");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));

    // After connect(), nextMsgId=1 then subscribe pre-increments before use.
    // So first subscribe uses msgId=2, second=3, third=4.

    // QoS 0 subscribe: topic="t", msgId=2
    byte sub0[] = { 0x82, 0x06, 0x00, 0x02, 0x00, 0x01, 0x74, 0x00 };
    shimClient.expect(sub0, sizeof(sub0));
    IS_TRUE(client.subscribe("t", 0));

    // QoS 1 subscribe: msgId=3
    byte sub1[] = { 0x82, 0x06, 0x00, 0x03, 0x00, 0x01, 0x74, 0x01 };
    shimClient.expect(sub1, sizeof(sub1));
    IS_TRUE(client.subscribe("t", 1));

    // QoS 2 subscribe: msgId=4
    byte sub2[] = { 0x82, 0x06, 0x00, 0x04, 0x00, 0x01, 0x74, 0x02 };
    shimClient.expect(sub2, sizeof(sub2));
    IS_TRUE(client.subscribe("t", 2));

    // QoS 3 — must reject
    IS_FALSE(client.subscribe("t", 3));

    IS_FALSE(shimClient.error());

    END_IT
}

// =========================================================================
// 16. QoS 1 retained publish
// =========================================================================

int test_qos1_retained_publish() {
    IT("QoS 1 retained publish sets RETAIN and QoS1 bits");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));

    /*
     * PUBLISH QoS1 + RETAIN: topic="t", payload="v", msgId=1
     *  0x33 = 0x30 (PUBLISH) | 0x02 (QoS1) | 0x01 (RETAIN)
     */
    byte expected[] = { 0x33, 0x06, 0x00, 0x01, 0x74, 0x00, 0x01, 0x76 };
    shimClient.expect(expected, sizeof(expected));

    bool rc = client.publish("t", (const uint8_t*)"v", 1, true, 1);
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());

    END_IT
}

// =========================================================================
// 17. Multiple in-flight QoS 1 messages (msgId increments)
// =========================================================================

int test_qos1_msgid_increments() {
    IT("consecutive QoS 1 publishes use incrementing msgIds");

    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    PubSubClient client(server, 1883, callback, shimClient);
    IS_TRUE(do_connect(client, shimClient));

    // First publish: msgId=1
    byte exp1[] = { 0x32, 0x06, 0x00, 0x01, 0x74, 0x00, 0x01, 0x76 };
    shimClient.expect(exp1, sizeof(exp1));
    IS_TRUE(client.publish("t", (const uint8_t*)"v", 1, false, 1));

    // Second publish: msgId=2
    byte exp2[] = { 0x32, 0x06, 0x00, 0x01, 0x74, 0x00, 0x02, 0x76 };
    shimClient.expect(exp2, sizeof(exp2));
    IS_TRUE(client.publish("t", (const uint8_t*)"v", 1, false, 1));

    IS_FALSE(shimClient.error());

    END_IT
}

// =========================================================================
// main
// =========================================================================

int main(int argc, char *argv[]) {
    SUITE("Improvements");

    test_macro_values();
    test_initial_state();
    test_qos1_publish_packet_format();
    test_qos2_publish_packet_format();
    test_qos2_outgoing_pubrec_pubrel();
    test_qos2_outgoing_pubcomp();
    test_receive_qos1_sends_puback();
    test_receive_qos2_sends_pubrec();
    test_receive_qos2_full_handshake();
    test_receive_qos2_duplicate_not_redelivered();
    test_publish_invalid_qos_rejected();
    test_publish_oversized_rejected();
    test_connect_invalid_will_qos_rejected();
    test_setbuffersize_preserves_old_on_failure();
    test_subscribe_qos_validation();
    test_qos1_retained_publish();
    test_qos1_msgid_increments();

    FINISH
}
