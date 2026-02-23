/*
 PubSubClient.h - A simple client for MQTT.
  Nick O'Leary
  http://knolleary.net
*/

#ifndef PubSubClient_h
#define PubSubClient_h

#include <Arduino.h>
#include "IPAddress.h"
#include "Client.h"
#include "Stream.h"

#define MQTT_VERSION_3_1      3
#define MQTT_VERSION_3_1_1    4
#define MQTT_VERSION_5        5

// MQTT_VERSION : Pick the version
//#define MQTT_VERSION MQTT_VERSION_3_1
#ifndef MQTT_VERSION
#define MQTT_VERSION MQTT_VERSION_3_1_1
#endif

// MQTT_ENABLE_V5 : Set to 0 to strip all MQTT 5 code at compile time (saves flash/RAM on 3.1.1-only builds)
#ifndef MQTT_ENABLE_V5
#define MQTT_ENABLE_V5 1
#endif

// MQTT_MAX_PACKET_SIZE : Maximum packet size. Override with setBufferSize().
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 256
#endif

// MQTT_KEEPALIVE : keepAlive interval in Seconds. Override with setKeepAlive()
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 15
#endif

// MQTT_SOCKET_TIMEOUT: socket timeout interval in Seconds. Override with setSocketTimeout()
#ifndef MQTT_SOCKET_TIMEOUT
#define MQTT_SOCKET_TIMEOUT 15
#endif

// MQTT_MAX_TRANSFER_SIZE : limit how much data is passed to the network client
//  in each write call. Needed for the Arduino Wifi Shield. Leave undefined to
//  pass the entire MQTT packet in each write call.
//#define MQTT_MAX_TRANSFER_SIZE 80

// Possible values for client.state()
#define MQTT_CONNECTION_TIMEOUT     -4
#define MQTT_CONNECTION_LOST        -3
#define MQTT_CONNECT_FAILED         -2
#define MQTT_DISCONNECTED           -1
#define MQTT_CONNECTED               0
#define MQTT_CONNECT_BAD_PROTOCOL    1
#define MQTT_CONNECT_BAD_CLIENT_ID   2
#define MQTT_CONNECT_UNAVAILABLE     3
#define MQTT_CONNECT_BAD_CREDENTIALS 4
#define MQTT_CONNECT_UNAUTHORIZED    5

#define MQTTCONNECT     (1 << 4)  // Client request to connect to Server
#define MQTTCONNACK     (2 << 4)  // Connect Acknowledgment
#define MQTTPUBLISH     (3 << 4)  // Publish message
#define MQTTPUBACK      (4 << 4)  // Publish Acknowledgment
#define MQTTPUBREC      (5 << 4)  // Publish Received (assured delivery part 1)
#define MQTTPUBREL      (6 << 4)  // Publish Release (assured delivery part 2)
#define MQTTPUBCOMP     (7 << 4)  // Publish Complete (assured delivery part 3)
#define MQTTSUBSCRIBE   (8 << 4)  // Client Subscribe request
#define MQTTSUBACK      (9 << 4)  // Subscribe Acknowledgment
#define MQTTUNSUBSCRIBE (10 << 4) // Client Unsubscribe request
#define MQTTUNSUBACK    (11 << 4) // Unsubscribe Acknowledgment
#define MQTTPINGREQ     (12 << 4) // PING Request
#define MQTTPINGRESP    (13 << 4) // PING Response
#define MQTTDISCONNECT  (14 << 4) // Client is Disconnecting
#define MQTTReserved    (15 << 4) // Reserved

#define MQTTQOS0        (0 << 1)
#define MQTTQOS1        (1 << 1)
#define MQTTQOS2        (2 << 1)

// Maximum size of fixed header and variable length size header
#define MQTT_MAX_HEADER_SIZE 5

// -------------------------------------------------------------------------
// MQTT 5 Property Identifiers (Section 2.2.2 of the MQTT 5 spec)
// -------------------------------------------------------------------------
#if MQTT_ENABLE_V5
#define MQTT_PROP_PAYLOAD_FORMAT          0x01  // Byte
#define MQTT_PROP_MESSAGE_EXPIRY          0x02  // Four Byte Integer
#define MQTT_PROP_CONTENT_TYPE            0x03  // UTF-8 String
#define MQTT_PROP_RESPONSE_TOPIC          0x08  // UTF-8 String
#define MQTT_PROP_CORRELATION_DATA        0x09  // Binary Data
#define MQTT_PROP_SUBSCRIPTION_IDENTIFIER 0x0B  // Variable Byte Integer
#define MQTT_PROP_SESSION_EXPIRY          0x11  // Four Byte Integer
#define MQTT_PROP_ASSIGNED_CLIENT_ID      0x12  // UTF-8 String
#define MQTT_PROP_SERVER_KEEP_ALIVE       0x13  // Two Byte Integer
#define MQTT_PROP_AUTH_METHOD             0x15  // UTF-8 String
#define MQTT_PROP_AUTH_DATA               0x16  // Binary Data
#define MQTT_PROP_REQUEST_PROBLEM_INFO    0x17  // Byte
#define MQTT_PROP_WILL_DELAY              0x18  // Four Byte Integer
#define MQTT_PROP_REQUEST_RESPONSE_INFO   0x19  // Byte
#define MQTT_PROP_RESPONSE_INFO           0x1A  // UTF-8 String
#define MQTT_PROP_SERVER_REFERENCE        0x1C  // UTF-8 String
#define MQTT_PROP_REASON_STRING           0x1F  // UTF-8 String
#define MQTT_PROP_RECEIVE_MAXIMUM         0x21  // Two Byte Integer
#define MQTT_PROP_TOPIC_ALIAS_MAX         0x22  // Two Byte Integer
#define MQTT_PROP_TOPIC_ALIAS             0x23  // Two Byte Integer
#define MQTT_PROP_MAX_QOS                 0x24  // Byte
#define MQTT_PROP_RETAIN_AVAILABLE        0x25  // Byte
#define MQTT_PROP_USER_PROPERTY           0x26  // UTF-8 String Pair (key + value)
#define MQTT_PROP_MAX_PACKET_SIZE         0x27  // Four Byte Integer
#define MQTT_PROP_WILDCARD_SUB_AVAILABLE  0x28  // Byte
#define MQTT_PROP_SUB_ID_AVAILABLE        0x29  // Byte
#define MQTT_PROP_SHARED_SUB_AVAILABLE    0x2A  // Byte
#endif // MQTT_ENABLE_V5

// QoS message tracking states (for in-flight QoS 1/2 messages)
#define MQTT_QOS_STATE_FREE          0
#define MQTT_QOS_STATE_WAIT_PUBACK   1  // Outgoing QoS 1: waiting for PUBACK
#define MQTT_QOS_STATE_WAIT_PUBREC   2  // Outgoing QoS 2: waiting for PUBREC
#define MQTT_QOS_STATE_WAIT_PUBCOMP  3  // Outgoing QoS 2: sent PUBREL, waiting for PUBCOMP
#define MQTT_QOS_STATE_WAIT_PUBREL   4  // Incoming QoS 2: sent PUBREC, waiting for PUBREL

// MQTT_MAX_QOS_PENDING : Maximum number of simultaneous in-flight QoS 1/2 messages
#ifndef MQTT_MAX_QOS_PENDING
#define MQTT_MAX_QOS_PENDING 8
#endif

struct PendingQoSMessage {
    uint16_t msgId;
    uint8_t state;
} __attribute__((packed));

#if defined(ESP8266) || defined(ESP32)
#include <functional>
#define MQTT_CALLBACK_SIGNATURE std::function<void(char*, uint8_t*, unsigned int)> callback
#else
#define MQTT_CALLBACK_SIGNATURE void (*callback)(char*, uint8_t*, unsigned int)
#endif

#define CHECK_STRING_LENGTH(l,s) if (l+2+strnlen(s, this->bufferSize) > this->bufferSize) {_client->stop();return false;}

class PubSubClient : public Print {
private:
   Client* _client;
   uint8_t* buffer;
   uint16_t bufferSize;
   uint16_t keepAlive;
   uint16_t socketTimeout;
   uint16_t nextMsgId;
   unsigned long lastOutActivity;
   unsigned long lastInActivity;
   bool pingOutstanding;
   MQTT_CALLBACK_SIGNATURE;
   PendingQoSMessage pendingMessages[MQTT_MAX_QOS_PENDING];
   uint32_t readPacket(uint8_t*);
   boolean readByte(uint8_t * result);
   boolean readByte(uint8_t * result, uint16_t * index);
   // Bulk-read 'count' bytes into buf with timeout; much faster than byte-by-byte
   boolean readBytes(uint8_t * buf, uint16_t count);
   boolean write(uint8_t header, uint8_t* buf, uint16_t length);
   uint16_t writeString(const char* string, uint8_t* buf, uint16_t pos);
   boolean sendSimplePacket(uint8_t type, uint16_t msgId);
   int8_t findPendingSlot(uint16_t msgId);
   int8_t findFreeSlot();
   void clearPendingMessages();
   void init(); // common constructor initialisation
   // --- Step 2: Variable Byte Integer helpers ---
   uint8_t  encodeVariableByteInteger(uint32_t value, uint8_t* buf);
   uint32_t decodeVariableByteInteger(const uint8_t* buf, uint8_t* bytesUsed);
#if MQTT_ENABLE_V5
   // --- Step 3: Property encoding/decoding helpers ---
   uint16_t writePropertyU8(uint8_t id, uint8_t value,        uint8_t* buf, uint16_t pos);
   uint16_t writePropertyU16(uint8_t id, uint16_t value,       uint8_t* buf, uint16_t pos);
   uint16_t writePropertyU32(uint8_t id, uint32_t value,       uint8_t* buf, uint16_t pos);
   uint16_t writePropertyStr(uint8_t id, const char* str,      uint8_t* buf, uint16_t pos);
   uint16_t writePropertyBin(uint8_t id, const uint8_t* data, uint16_t len, uint8_t* buf, uint16_t pos);
   uint16_t writePropertyVBI(uint8_t id, uint32_t value,       uint8_t* buf, uint16_t pos);
   // Advance pos past entire properties section (length VBI + bytes)
   uint16_t skipProperties(const uint8_t* buf, uint16_t pos);
   // Advance pos past a single property's value (used internally by findProperty)
   uint16_t skipPropertyValue(uint8_t propId, const uint8_t* buf, uint16_t pos);
   // Find a property by id; sets *valueOut to the start of its value, *valueLenOut to its encoded byte size
   bool     findProperty(const uint8_t* buf, uint16_t propsPayloadStart, uint16_t propsPayloadEnd,
                         uint8_t id, const uint8_t** valueOut, uint16_t* valueLenOut);
   uint8_t  _mqttVersion;         // runtime-selected protocol version (3, 4, or 5)
   uint16_t _v5ServerReceiveMax;  // Step 5: broker's Receive Maximum (default 65535)
   uint16_t _v5ServerKeepalive;   // Step 5: broker's Server Keep Alive (0 = unset)
#endif // MQTT_ENABLE_V5
   // Build up the header ready to send
   // Returns the size of the header
   // Note: the header is built at the end of the first MQTT_MAX_HEADER_SIZE bytes, so will start
   //       (MQTT_MAX_HEADER_SIZE - <returned size>) bytes into the buffer
   size_t buildHeader(uint8_t header, uint8_t* buf, uint16_t length);
   IPAddress ip;
   const char* domain;
   uint16_t port;
   Stream* stream;
   int8_t _state; // values fit in [-4..5], saves 1-3 bytes vs int
public:
   PubSubClient();
   PubSubClient(Client& client);
   PubSubClient(IPAddress, uint16_t, Client& client);
   PubSubClient(IPAddress, uint16_t, Client& client, Stream&);
   PubSubClient(IPAddress, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client);
   PubSubClient(IPAddress, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client, Stream&);
   PubSubClient(uint8_t *, uint16_t, Client& client);
   PubSubClient(uint8_t *, uint16_t, Client& client, Stream&);
   PubSubClient(uint8_t *, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client);
   PubSubClient(uint8_t *, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client, Stream&);
   PubSubClient(const char*, uint16_t, Client& client);
   PubSubClient(const char*, uint16_t, Client& client, Stream&);
   PubSubClient(const char*, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client);
   PubSubClient(const char*, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client, Stream&);

   ~PubSubClient();

   PubSubClient& setServer(IPAddress ip, uint16_t port);
   PubSubClient& setServer(uint8_t * ip, uint16_t port);
   PubSubClient& setServer(const char * domain, uint16_t port);
   PubSubClient& setCallback(MQTT_CALLBACK_SIGNATURE);
   PubSubClient& setClient(Client& client);
   PubSubClient& setStream(Stream& stream);
   PubSubClient& setKeepAlive(uint16_t keepAlive);
   PubSubClient& setSocketTimeout(uint16_t timeout);
#if MQTT_ENABLE_V5
   // Set/get the MQTT protocol version used for the next connect() call.
   // Accepted values: MQTT_VERSION_3_1 (3), MQTT_VERSION_3_1_1 (4), MQTT_VERSION_5 (5)
   PubSubClient& setMqttVersion(uint8_t version);
   uint8_t       getMqttVersion() const;
#endif // MQTT_ENABLE_V5

   boolean setBufferSize(uint16_t size);
   uint16_t getBufferSize();

   boolean connect(const char* id);
   boolean connect(const char* id, const char* user, const char* pass);
   boolean connect(const char* id, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
   boolean connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
   boolean connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage, boolean cleanSession);
   void disconnect();
   boolean publish(const char* topic, const char* payload);
   boolean publish(const char* topic, const char* payload, boolean retained);
   boolean publish(const char* topic, const uint8_t * payload, unsigned int plength);
   boolean publish(const char* topic, const uint8_t * payload, unsigned int plength, boolean retained);
   // Publish with explicit QoS level (0, 1, or 2)
   boolean publish(const char* topic, const char* payload, boolean retained, uint8_t qos);
   boolean publish(const char* topic, const uint8_t * payload, unsigned int plength, boolean retained, uint8_t qos);
   boolean publish_P(const char* topic, const char* payload, boolean retained);
   boolean publish_P(const char* topic, const uint8_t * payload, unsigned int plength, boolean retained);
   // Start to publish a message.
   // This API:
   //   beginPublish(...)
   //   one or more calls to write(...)
   //   endPublish()
   // Allows for arbitrarily large payloads to be sent without them having to be copied into
   // a new buffer and held in memory at one time
   // Returns 1 if the message was started successfully, 0 if there was an error
   boolean beginPublish(const char* topic, unsigned int plength, boolean retained);
   // Finish off this publish message (started with beginPublish)
   // Returns 1 if the packet was sent successfully, 0 if there was an error
   int endPublish();
   // Write a single byte of payload (only to be used with beginPublish/endPublish)
   virtual size_t write(uint8_t);
   // Write size bytes from buffer into the payload (only to be used with beginPublish/endPublish)
   // Returns the number of bytes written
   virtual size_t write(const uint8_t *buffer, size_t size);
   boolean subscribe(const char* topic);
   boolean subscribe(const char* topic, uint8_t qos);
   boolean unsubscribe(const char* topic);
   boolean loop();
   boolean connected();
   int state();

};


#endif
