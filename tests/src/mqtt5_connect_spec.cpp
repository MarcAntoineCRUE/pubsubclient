/**
 * @file mqtt5_connect_spec.cpp
 * @brief Tests for MQTT 5 CONNECT / CONNACK packet handling (Step 4).
 *
 * Verifies:
 *  - CONNECT byte layout: protocol name "MQTT", level 0x05, Connect Properties (length=0)
 *  - Will Properties section present when will is configured
 *  - CONNACK Reason Code 0x00 → MQTT_CONNECTED
 *  - CONNACK failure Reason Codes → correct library state
 *  - Backward compat: MQTT 3.1.1 behaviour unchanged when version != 5
 */
#include "BDDTest.h"
#include "Buffer.h"
#include "PubSubClient.h"
#include "ShimClient.h"
#include "trace.h"

byte server[] = {172, 16, 0, 2};

// function declarations
void callback(char* topic, uint8_t* payload, size_t plength);
int test_mqtt5_connect_properly_formatted();
int test_mqtt5_connect_with_username_password();
int test_mqtt5_connect_with_will();
int test_mqtt5_connect_with_will_and_user();
int test_mqtt5_connack_success();
int test_mqtt5_connack_bad_credentials();
int test_mqtt5_connack_server_unavailable();
int test_mqtt5_connack_bad_protocol();
int test_mqtt5_connack_not_authorized();
int test_mqtt311_connect_unchanged_when_version_default();
int test_mqtt311_connect_unchanged_when_explicitly_set();

void callback(_UNUSED_ char* topic, _UNUSED_ uint8_t* payload, _UNUSED_ size_t plength) {}

// ──────────────────────────────────────────────────────────────────────────────
// MQTT 5 CONNECT packet format
// ──────────────────────────────────────────────────────────────────────────────

int test_mqtt5_connect_properly_formatted() {
    IT("MQTT5: sends a properly formatted CONNECT packet (no will, no user/pass)");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);
    byte expectServer[] = {172, 16, 0, 2};
    shimClient.expectConnect(expectServer, 1883);

    // Expected bytes:
    //   0x10 0x19                                 — header + remaining_length = 25
    //   0x00 0x04 'M' 'Q' 'T' 'T' 0x05           — protocol name + level 5
    //   0x02                                       — connect flags (CleanStart)
    //   0x00 0x0F                                  — KeepAlive = 15
    //   0x00                                       — Connect Properties length = 0
    //   0x00 0x0C "client_test1"                   — ClientId (12 chars)
    byte connect5[] = {
        0x10, 0x19,                                        // header, remaining 25
        0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x05,        // "MQTT" + level 5
        0x02,                                              // flags: CleanStart
        0x00, 0x0F,                                        // KeepAlive = 15
        0x00,                                              // Connect Properties length = 0
        0x00, 0x0C,                                        // ClientId length = 12
        0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x5F, 0x74, 0x65, 0x73, 0x74, 0x31  // "client_test1"
    };
    // Minimal MQTT5 CONNACK: session_present=0, reason_code=0, props_len=0
    byte connack5[] = {0x20, 0x03, 0x00, 0x00, 0x00};

    shimClient.expect(connect5, (int)sizeof(connect5));
    shimClient.respond(connack5, (int)sizeof(connack5));

    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);

    bool rc = client.connect("client_test1");
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());
    IS_EQUAL(client.state(), (int)MQTT_CONNECTED);
    END_IT
}

int test_mqtt5_connect_with_username_password() {
    IT("MQTT5: CONNECT with username and password includes correct flags");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    // Flags: CleanStart(0x02) | UserName(0x80) | Password(0x40) = 0xC2
    // Remaining = 7+1+2+1+14+6+6 = 37 = 0x25
    byte connect5[] = {
        0x10, 0x25,                                         // header, remaining 37
        0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x05,         // "MQTT" + level 5
        0xC2,                                               // flags: CleanStart + User + Pass
        0x00, 0x0F,                                         // KeepAlive = 15
        0x00,                                               // Connect Properties length = 0
        0x00, 0x0C,                                         // ClientId length = 12
        0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x5F, 0x74, 0x65, 0x73, 0x74, 0x31,  // "client_test1"
        0x00, 0x04, 0x75, 0x73, 0x65, 0x72,                // username "user"
        0x00, 0x04, 0x70, 0x61, 0x73, 0x73                 // password "pass"
    };
    byte connack5[] = {0x20, 0x03, 0x00, 0x00, 0x00};

    shimClient.expect(connect5, (int)sizeof(connect5));
    shimClient.respond(connack5, (int)sizeof(connack5));

    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);

    bool rc = client.connect("client_test1", "user", "pass");
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());
    END_IT
}

int test_mqtt5_connect_with_will() {
    IT("MQTT5: CONNECT with will includes Will Properties section (length=0)");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    // Flags: CleanStart(0x02) | Will(0x04) = 0x06
    // Remaining = 7+1+2+1+14+1+5+5 = 36 = 0x24
    byte connect5[] = {
        0x10, 0x24,                                         // header, remaining 36
        0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x05,         // "MQTT" + level 5
        0x06,                                               // flags: CleanStart + Will
        0x00, 0x0F,                                         // KeepAlive = 15
        0x00,                                               // Connect Properties length = 0
        0x00, 0x0C,                                         // ClientId length = 12
        0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x5F, 0x74, 0x65, 0x73, 0x74, 0x31,  // "client_test1"
        0x00,                                               // Will Properties length = 0
        0x00, 0x03, 0x74, 0x2F, 0x77,                      // will topic "t/w"
        0x00, 0x03, 0x6D, 0x73, 0x67                       // will message "msg"
    };
    byte connack5[] = {0x20, 0x03, 0x00, 0x00, 0x00};

    shimClient.expect(connect5, (int)sizeof(connect5));
    shimClient.respond(connack5, (int)sizeof(connack5));

    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);

    bool rc = client.connect("client_test1", "t/w", MQTT_QOS0, false, "msg");
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());
    END_IT
}

int test_mqtt5_connect_with_will_and_user() {
    IT("MQTT5: CONNECT with will + username/password includes all sections");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    // Flags: CleanStart(0x02) | Will(0x04) | User(0x80) | Pass(0x40) = 0xC6
    // Remaining = 7+1+2+1+14+1+5+5+6+6 = 48 = 0x30
    byte connect5[] = {
        0x10, 0x30,                                         // header, remaining 48
        0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x05,         // "MQTT" + level 5
        0xC6,                                               // flags: CleanStart+Will+User+Pass
        0x00, 0x0F,                                         // KeepAlive = 15
        0x00,                                               // Connect Properties length = 0
        0x00, 0x0C,                                         // ClientId length = 12
        0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x5F, 0x74, 0x65, 0x73, 0x74, 0x31,  // "client_test1"
        0x00,                                               // Will Properties length = 0
        0x00, 0x03, 0x74, 0x2F, 0x77,                      // will topic "t/w"
        0x00, 0x03, 0x6D, 0x73, 0x67,                      // will message "msg"
        0x00, 0x04, 0x75, 0x73, 0x65, 0x72,                // username "user"
        0x00, 0x04, 0x70, 0x61, 0x73, 0x73                 // password "pass"
    };
    byte connack5[] = {0x20, 0x03, 0x00, 0x00, 0x00};

    shimClient.expect(connect5, (int)sizeof(connect5));
    shimClient.respond(connack5, (int)sizeof(connack5));

    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);

    bool rc = client.connect("client_test1", "user", "pass", "t/w", MQTT_QOS0, false, "msg", true);
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());
    END_IT
}

// ──────────────────────────────────────────────────────────────────────────────
// MQTT 5 CONNACK Reason Code mapping
// ──────────────────────────────────────────────────────────────────────────────

int test_mqtt5_connack_success() {
    IT("MQTT5: CONNACK reason code 0x00 → MQTT_CONNECTED");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);
    byte connack5[] = {0x20, 0x03, 0x00, 0x00, 0x00};
    shimClient.respond(connack5, 5);

    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    bool rc = client.connect("c");
    IS_TRUE(rc);
    IS_EQUAL(client.state(), (int)MQTT_CONNECTED);
    END_IT
}

int test_mqtt5_connack_bad_credentials() {
    IT("MQTT5: CONNACK reason code 0x86 → MQTT_CONNECT_BAD_CREDENTIALS");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);
    byte connack5[] = {0x20, 0x03, 0x00, (byte)MQTT5_REASON_BAD_CREDENTIALS, 0x00};
    shimClient.respond(connack5, 5);

    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    bool rc = client.connect("c");
    IS_FALSE(rc);
    IS_EQUAL(client.state(), (int)MQTT_CONNECT_BAD_CREDENTIALS);
    END_IT
}

int test_mqtt5_connack_server_unavailable() {
    IT("MQTT5: CONNACK reason code 0x88 → MQTT_CONNECT_UNAVAILABLE");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);
    byte connack5[] = {0x20, 0x03, 0x00, (byte)MQTT5_REASON_SERVER_UNAVAILABLE, 0x00};
    shimClient.respond(connack5, 5);

    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    bool rc = client.connect("c");
    IS_FALSE(rc);
    IS_EQUAL(client.state(), (int)MQTT_CONNECT_UNAVAILABLE);
    END_IT
}

int test_mqtt5_connack_bad_protocol() {
    IT("MQTT5: CONNACK reason code 0x84 → MQTT_CONNECT_BAD_PROTOCOL");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);
    byte connack5[] = {0x20, 0x03, 0x00, (byte)MQTT5_REASON_UNSUPPORTED_PROTOCOL, 0x00};
    shimClient.respond(connack5, 5);

    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    bool rc = client.connect("c");
    IS_FALSE(rc);
    IS_EQUAL(client.state(), (int)MQTT_CONNECT_BAD_PROTOCOL);
    END_IT
}

int test_mqtt5_connack_not_authorized() {
    IT("MQTT5: CONNACK reason code 0x87 → MQTT_CONNECT_UNAUTHORIZED");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);
    byte connack5[] = {0x20, 0x03, 0x00, (byte)MQTT5_REASON_NOT_AUTHORIZED, 0x00};
    shimClient.respond(connack5, 5);

    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);
    bool rc = client.connect("c");
    IS_FALSE(rc);
    IS_EQUAL(client.state(), (int)MQTT_CONNECT_UNAUTHORIZED);
    END_IT
}

// ──────────────────────────────────────────────────────────────────────────────
// Backward compatibility: MQTT 3.1.1 behaviour must be unchanged
// ──────────────────────────────────────────────────────────────────────────────

int test_mqtt311_connect_unchanged_when_version_default() {
    IT("backward compat: MQTT 3.1.1 CONNECT unchanged when version is default");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);
    byte expectServer[] = {172, 16, 0, 2};
    shimClient.expectConnect(expectServer, 1883);

    // Exact same bytes as the existing connect_spec test
    byte connect311[] = {
        0x10, 0x18,
        0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x04,
        0x02, 0x00, 0x0F,
        0x00, 0x0C,
        0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x5F, 0x74, 0x65, 0x73, 0x74, 0x31
    };
    byte connack[] = {0x20, 0x02, 0x00, 0x00};

    shimClient.expect(connect311, 26);
    shimClient.respond(connack, 4);

    // Client uses default version (MQTT 3.1.1 at compile time)
    PubSubClient client(server, 1883, callback, shimClient);

    bool rc = client.connect("client_test1");
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());
    IS_EQUAL(client.state(), (int)MQTT_CONNECTED);
    END_IT
}

int test_mqtt311_connect_unchanged_when_explicitly_set() {
    IT("backward compat: MQTT 3.1.1 CONNECT unchanged when explicitly set to 3.1.1");
    ShimClient shimClient;
    shimClient.setAllowConnect(true);

    byte connect311[] = {
        0x10, 0x18,
        0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x04,
        0x02, 0x00, 0x0F,
        0x00, 0x0C,
        0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x5F, 0x74, 0x65, 0x73, 0x74, 0x31
    };
    byte connack[] = {0x20, 0x02, 0x00, 0x00};

    shimClient.expect(connect311, 26);
    shimClient.respond(connack, 4);

    PubSubClient client(server, 1883, callback, shimClient);
    client.setMqttVersion(MQTT_VERSION_5);     // temporarily 5
    client.setMqttVersion(MQTT_VERSION_3_1_1); // back to 3.1.1

    bool rc = client.connect("client_test1");
    IS_TRUE(rc);
    IS_FALSE(shimClient.error());
    END_IT
}

// ──────────────────────────────────────────────────────────────────────────────
// main
// ──────────────────────────────────────────────────────────────────────────────

int main() {
    SUITE("MQTT 5 CONNECT / CONNACK");

    // CONNECT packet format
    test_mqtt5_connect_properly_formatted();
    test_mqtt5_connect_with_username_password();
    test_mqtt5_connect_with_will();
    test_mqtt5_connect_with_will_and_user();

    // CONNACK Reason Code handling
    test_mqtt5_connack_success();
    test_mqtt5_connack_bad_credentials();
    test_mqtt5_connack_server_unavailable();
    test_mqtt5_connack_bad_protocol();
    test_mqtt5_connack_not_authorized();

    // Backward compatibility
    test_mqtt311_connect_unchanged_when_version_default();
    test_mqtt311_connect_unchanged_when_explicitly_set();

    FINISH
}
