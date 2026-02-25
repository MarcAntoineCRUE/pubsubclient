/**
 * @file PubSubClient.h
 * @brief A simple client for MQTT.
 * @author Nicholas O'Leary - http://knolleary.net
 * @author Holger Mueller - https://github.com/hmueller01/pubsubclient3
 * @copyright MIT License 2008-2025
 *
 * This file is part of the PubSubClient library.
 */

#ifndef PubSubClient_h
#define PubSubClient_h

#include <Arduino.h>

#include "Client.h"
#include "IPAddress.h"
#include "Stream.h"

#define MQTT_VERSION_3_1   3  ///< Defines MQTT 3.1 protocol version, see #MQTT_VERSION
#define MQTT_VERSION_3_1_1 4  ///< Defines MQTT 3.1.1 protocol version, see #MQTT_VERSION
#define MQTT_VERSION_5     5  ///< Defines MQTT 5.0 protocol version, see #MQTT_VERSION

//< @note The following #define directives can be used to configure the library.

/**
 * @brief Sets the version of the MQTT protocol to use (3.1 or 3.1.1). [#MQTT_VERSION_3_1, #MQTT_VERSION_3_1_1].
 * @note Default value is #MQTT_VERSION_3_1_1 for MQTT 3.1.1.
 */
#ifndef MQTT_VERSION
#define MQTT_VERSION MQTT_VERSION_3_1_1
#endif

/**
 * @brief Maximum packet size defined by MQTT protocol.
 */
#ifndef MQTT_MAX_POSSIBLE_PACKET_SIZE
#define MQTT_MAX_POSSIBLE_PACKET_SIZE ((size_t)268435455)  // might be limited to 65535 if size_t is 16-bit (unsigned int)
#endif

/**
 * @brief Sets the largest packet size, in bytes, the client will handle.
 * Any packet received that exceeds this size will be ignored.
 * This value can be overridden by calling setBufferSize.
 * @note Default value is 256 bytes.
 */
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 256
#endif

/**
 * @brief Sets the keepalive interval, in seconds, the client will use.
 * This is used to maintain the connection when no other packets are being sent or received.
 * This value can be overridden by calling setKeepAlive.
 * @note Default value is 15 seconds.
 */
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 15
#endif

/**
 * @brief Sets the timeout, in seconds, when reading from the network.
 * This also applies as the timeout for calls to connect.
 * This value can be overridden by calling setSocketTimeout.
 * @note Default value is 15 seconds.
 */
#ifndef MQTT_SOCKET_TIMEOUT
#define MQTT_SOCKET_TIMEOUT 15
#endif

/**
 * @brief Sets the maximum number of bytes passed to the network client in each write call.
 * Some hardware has a limit to how much data can be passed to them in one go,
 * such as the Arduino Wifi Shield e.g. use 80.
 * @note Defaults to undefined, which passes the entire packet in each write call.
 */
#ifndef MQTT_MAX_TRANSFER_SIZE  // just a hack that it gets shown in Doxygen
#define MQTT_MAX_TRANSFER_SIZE 80
#undef MQTT_MAX_TRANSFER_SIZE
#endif

/**
 * @defgroup group_state state() result
 * @brief These values indicate the current PubSubClient::state() of the client.
 * @{
 */
#define MQTT_CONNECTION_TIMEOUT      -4  ///< The network connection timed out or server didn't respond within the keepalive time.
#define MQTT_CONNECTION_LOST         -3  ///< The network connection was lost/broken.
#define MQTT_CONNECT_FAILED          -2  ///< The network connection failed.
#define MQTT_DISCONNECTED            -1  ///< The client is disconnected cleanly.
#define MQTT_CONNECTED               0   ///< The client is connected.
#define MQTT_CONNECT_BAD_PROTOCOL    1   ///< The server does not support the requested MQTT version.
#define MQTT_CONNECT_BAD_CLIENT_ID   2   ///< The server rejected the client identifier.
#define MQTT_CONNECT_UNAVAILABLE     3   ///< The server was unable to accept the connection.
#define MQTT_CONNECT_BAD_CREDENTIALS 4   ///< The username or password is not valid.
#define MQTT_CONNECT_UNAUTHORIZED    5   ///< The client is not authorized to connect to the server.
/** @} */

/**
 * @defgroup group_mqtt5_reason MQTT 5 Reason Codes (CONNACK)
 * @brief MQTT 5 specific reason codes returned in CONNACK packets (values >= 0x80 indicate failure).
 * @{
 */
#define MQTT5_REASON_SUCCESS                    0x00  ///< Connection accepted.
#define MQTT5_REASON_UNSPECIFIED_ERROR          0x80  ///< Unspecified error.
#define MQTT5_REASON_MALFORMED_PACKET           0x81  ///< Malformed packet.
#define MQTT5_REASON_PROTOCOL_ERROR             0x82  ///< Protocol error.
#define MQTT5_REASON_IMPL_SPECIFIC_ERROR        0x83  ///< Implementation specific error.
#define MQTT5_REASON_UNSUPPORTED_PROTOCOL       0x84  ///< Unsupported protocol version.
#define MQTT5_REASON_CLIENT_ID_INVALID          0x85  ///< Client identifier not valid.
#define MQTT5_REASON_BAD_CREDENTIALS            0x86  ///< Bad username or password.
#define MQTT5_REASON_NOT_AUTHORIZED             0x87  ///< Not authorized.
#define MQTT5_REASON_SERVER_UNAVAILABLE         0x88  ///< Server unavailable.
#define MQTT5_REASON_SERVER_BUSY                0x89  ///< Server busy.
#define MQTT5_REASON_BANNED                     0x8A  ///< Banned.
#define MQTT5_REASON_BAD_AUTH_METHOD            0x8C  ///< Bad authentication method.
#define MQTT5_REASON_TOPIC_NAME_INVALID         0x90  ///< Topic name invalid.
#define MQTT5_REASON_PACKET_TOO_LARGE           0x95  ///< Packet too large.
#define MQTT5_REASON_QUOTA_EXCEEDED             0x97  ///< Quota exceeded.
#define MQTT5_REASON_PAYLOAD_FORMAT_INVALID     0x99  ///< Payload format invalid.
#define MQTT5_REASON_RETAIN_NOT_SUPPORTED       0x9A  ///< Retain not supported.
#define MQTT5_REASON_QOS_NOT_SUPPORTED          0x9B  ///< QoS not supported.
#define MQTT5_REASON_USE_ANOTHER_SERVER         0x9C  ///< Use another server.
#define MQTT5_REASON_SERVER_MOVED               0x9D  ///< Server moved.
#define MQTT5_REASON_CONNECTION_RATE_EXCEEDED   0x9F  ///< Connection rate exceeded.
/** @} */

/**
 * @defgroup group_mqtt5_props MQTT 5 Property Identifiers
 * @brief Property identifiers used in MQTT 5 packets.
 * @{
 */
#define MQTT_PROP_PAYLOAD_FORMAT        ((uint8_t)0x01)  ///< Payload Format Indicator (Byte)
#define MQTT_PROP_MESSAGE_EXPIRY        ((uint8_t)0x02)  ///< Message Expiry Interval (Four Byte Integer)
#define MQTT_PROP_CONTENT_TYPE          ((uint8_t)0x03)  ///< Content Type (UTF-8 Encoded String)
#define MQTT_PROP_RESPONSE_TOPIC        ((uint8_t)0x08)  ///< Response Topic (UTF-8 Encoded String)
#define MQTT_PROP_CORRELATION_DATA      ((uint8_t)0x09)  ///< Correlation Data (Binary Data)
#define MQTT_PROP_SUBSCRIPTION_ID       ((uint8_t)0x0B)  ///< Subscription Identifier (Variable Byte Integer)
#define MQTT_PROP_SESSION_EXPIRY        ((uint8_t)0x11)  ///< Session Expiry Interval (Four Byte Integer)
#define MQTT_PROP_ASSIGNED_CLIENT_ID    ((uint8_t)0x12)  ///< Assigned Client Identifier (UTF-8 Encoded String)
#define MQTT_PROP_SERVER_KEEPALIVE      ((uint8_t)0x13)  ///< Server Keep Alive (Two Byte Integer)
#define MQTT_PROP_AUTH_METHOD           ((uint8_t)0x15)  ///< Authentication Method (UTF-8 Encoded String)
#define MQTT_PROP_AUTH_DATA             ((uint8_t)0x16)  ///< Authentication Data (Binary Data)
#define MQTT_PROP_REQUEST_PROBLEM_INFO  ((uint8_t)0x17)  ///< Request Problem Information (Byte)
#define MQTT_PROP_WILL_DELAY            ((uint8_t)0x18)  ///< Will Delay Interval (Four Byte Integer)
#define MQTT_PROP_REQUEST_RESPONSE_INFO ((uint8_t)0x19)  ///< Request Response Information (Byte)
#define MQTT_PROP_RESPONSE_INFO         ((uint8_t)0x1A)  ///< Response Information (UTF-8 Encoded String)
#define MQTT_PROP_SERVER_REFERENCE      ((uint8_t)0x1C)  ///< Server Reference (UTF-8 Encoded String)
#define MQTT_PROP_REASON_STRING         ((uint8_t)0x1F)  ///< Reason String (UTF-8 Encoded String)
#define MQTT_PROP_RECEIVE_MAXIMUM       ((uint8_t)0x21)  ///< Receive Maximum (Two Byte Integer)
#define MQTT_PROP_TOPIC_ALIAS_MAX       ((uint8_t)0x22)  ///< Topic Alias Maximum (Two Byte Integer)
#define MQTT_PROP_TOPIC_ALIAS           ((uint8_t)0x23)  ///< Topic Alias (Two Byte Integer)
#define MQTT_PROP_MAX_QOS               ((uint8_t)0x24)  ///< Maximum QoS (Byte)
#define MQTT_PROP_RETAIN_AVAILABLE      ((uint8_t)0x25)  ///< Retain Available (Byte)
#define MQTT_PROP_USER_PROPERTY         ((uint8_t)0x26)  ///< User Property (UTF-8 String Pair)
#define MQTT_PROP_MAXIMUM_PACKET_SIZE   ((uint8_t)0x27)  ///< Maximum Packet Size (Four Byte Integer)
#define MQTT_PROP_WILDCARD_SUB_AVAIL    ((uint8_t)0x28)  ///< Wildcard Subscription Available (Byte)
#define MQTT_PROP_SUB_ID_AVAIL          ((uint8_t)0x29)  ///< Subscription Identifier Available (Byte)
#define MQTT_PROP_SHARED_SUB_AVAIL      ((uint8_t)0x2A)  ///< Shared Subscription Available (Byte)
/** @} */

/// \cond
#define MQTTRETAINED    1        // Retained flag in the header
#define MQTTCONNECT     1 << 4   // Client request to connect to Server
#define MQTTCONNACK     2 << 4   // Connect Acknowledgment
#define MQTTPUBLISH     3 << 4   // Publish message
#define MQTTPUBACK      4 << 4   // Publish Acknowledgment
#define MQTTPUBREC      5 << 4   // Publish Received (assured delivery part 1)
#define MQTTPUBREL      6 << 4   // Publish Release (assured delivery part 2)
#define MQTTPUBCOMP     7 << 4   // Publish Complete (assured delivery part 3)
#define MQTTSUBSCRIBE   8 << 4   // Client Subscribe request
#define MQTTSUBACK      9 << 4   // Subscribe Acknowledgment
#define MQTTUNSUBSCRIBE 10 << 4  // Client Unsubscribe request
#define MQTTUNSUBACK    11 << 4  // Unsubscribe Acknowledgment
#define MQTTPINGREQ     12 << 4  // PING Request
#define MQTTPINGRESP    13 << 4  // PING Response
#define MQTTDISCONNECT  14 << 4  // Client is Disconnecting
#define MQTTAUTH        15 << 4  // Authentication (MQTT 5 only)
/// \endcond

/**
 * @defgroup group_qos QoS levels
 * @brief Quality of Service (QoS) levels for MQTT messages.
 * @{
 */
#define MQTT_QOS0 ((uint8_t)0)  ///< Quality of Service 0: At most once
#define MQTT_QOS1 ((uint8_t)1)  ///< Quality of Service 1: At least once
#define MQTT_QOS2 ((uint8_t)2)  ///< Quality of Service 2: Exactly once
/// \cond
#define MQTT_QOS_GET_HDR(qos) (((qos) & 0x03) << 1) // Get QoS header bits from QoS value
#define MQTT_HDR_GET_QOS(header) (((header) & 0x06 ) >> 1) // Get QoS value from MQTT header
/// \endcond
/** @} */

/// \cond Maximum size of fixed header and variable length size header
#define MQTT_MAX_HEADER_SIZE 5
/// \endcond

/// \anchor callback
/**
 * @brief Define the signature required by any callback function.
 * @param topic The topic of the message.
 * @param payload The payload of the message.
 * @param plength The length of the payload.
 */
#if defined(__has_include) && __has_include(<functional>) && !defined(NOFUNCTIONAL)
#include <functional>
#define MQTT_CALLBACK_SIGNATURE std::function<void(char* topic, uint8_t* payload, size_t plength)> callback
#else
#define MQTT_CALLBACK_SIGNATURE void (*callback)(char* topic, uint8_t* payload, size_t plength)
#endif

/// \cond
#ifdef DEBUG_ESP_PORT
#ifdef DEBUG_PUBSUBCLIENT
#define DEBUG_PSC_PRINTF(fmt, ...) DEBUG_ESP_PORT.printf(("PubSubClient: " fmt), ##__VA_ARGS__)
#else
#define DEBUG_PSC_PRINTF(...)
#endif

#define ERROR_PSC_PRINTF(fmt, ...) DEBUG_ESP_PORT.printf(("PubSubClient error: " fmt), ##__VA_ARGS__)
#define ERROR_PSC_PRINTF_P(fmt, ...) DEBUG_ESP_PORT.printf_P(PSTR("PubSubClient error: " fmt), ##__VA_ARGS__)
#else  // DEBUG_ESP_PORT
#ifndef DEBUG_PSC_PRINTF
#define DEBUG_PSC_PRINTF(...)
#endif
#ifndef ERROR_PSC_PRINTF
#define ERROR_PSC_PRINTF(fmt, ...)
#endif
#ifndef ERROR_PSC_PRINTF_P
#define ERROR_PSC_PRINTF_P(fmt, ...)
#endif
#endif
/// \endcond

/**
 * @class PubSubClient
 * @brief This class provides a client for doing simple publish and subscribe messaging with a server that supports MQTT.
 */
class PubSubClient : public Print {
   private:
    Client* _client{};
    uint8_t* _buffer{};
    size_t _bufferSize{};
    size_t _bufferWritePos{};
    unsigned long _keepAliveMillis{};
    unsigned long _socketTimeoutMillis{};
    uint16_t _nextMsgId{};
    unsigned long _lastOutActivity{};
    unsigned long _lastInActivity{};
    bool _pingOutstanding{};
    MQTT_CALLBACK_SIGNATURE{};
    IPAddress _ip{};
    char* _domain{};
    uint16_t _port{};
    Stream* _stream{};
    int _state{MQTT_DISCONNECTED};
    uint8_t _mqttVersion{MQTT_VERSION};  ///< Runtime MQTT protocol version (default: compile-time #MQTT_VERSION)

    size_t readPacket(uint8_t* hdrLen);
    bool handlePacket(uint8_t hdrLen, size_t len);
    bool readByte(uint8_t* result);
    bool readByte(uint8_t* result, size_t* pos);
    uint8_t buildHeader(uint8_t header, size_t length);
    bool writeControlPacket(uint8_t header, size_t length);
    size_t writeBuffer(size_t pos, size_t size);
    size_t writeStringImpl(bool progmem, const char* string, size_t pos);
    size_t writeString(const char* string, size_t pos);
    size_t writeNextMsgId(size_t pos);

    bool beginPublishImpl(bool progmem, const char* topic, size_t plength, uint8_t qos, bool retained);
    bool subscribeImpl(bool progmem, const char* topic, uint8_t qos);
    bool unsubscribeImpl(bool progmem, const char* topic);

    // Add to buffer and flush if full (only to be used with beginPublish/endPublish)
    size_t appendBuffer(uint8_t data);
    size_t flushBuffer();

    // --- MQTT 5: Variable Byte Integer helpers ---
    /**
     * @brief Encodes a 32-bit value as a Variable Byte Integer into buf.
     * @param value The value to encode (0 .. 268435455).
     * @param buf   Destination buffer of at least 4 bytes.
     * @return Number of bytes written (1..4), or 0 on overflow.
     */
    uint8_t encodeVariableByteInteger(uint32_t value, uint8_t* buf);

    /**
     * @brief Decodes a Variable Byte Integer from buf.
     * @param buf       Source buffer.
     * @param bytesUsed Set to the number of bytes consumed (1..4).
     * @return Decoded value.
     */
    uint32_t decodeVariableByteInteger(const uint8_t* buf, uint8_t* bytesUsed);

    // --- MQTT 5: Property encoding helpers ---
    /** Write raw property [id][value bytes] into _buffer at pos. Returns new pos. */
    size_t writePropertyRaw(uint8_t id, const uint8_t* value, size_t len, size_t pos);
    /** Write 1-byte property. */
    size_t writePropertyU8(uint8_t id, uint8_t value, size_t pos);
    /** Write 2-byte big-endian property. */
    size_t writePropertyU16(uint8_t id, uint16_t value, size_t pos);
    /** Write 4-byte big-endian property. */
    size_t writePropertyU32(uint8_t id, uint32_t value, size_t pos);
    /** Write UTF-8 string property (2-byte length prefix + bytes). */
    size_t writePropertyStr(uint8_t id, const char* value, size_t pos);
    /** Write VBI-encoded property. */
    size_t writePropertyVBI(uint8_t id, uint32_t value, size_t pos);

    /**
     * @brief Advance pos past the entire properties section starting with a VBI length.
     * @param pos Position in _buffer where the property length VBI starts.
     * @return Position immediately after the properties section, or pos on error.
     */
    size_t skipProperties(size_t pos);

    /**
     * @brief Scan the properties section for a specific property ID.
     * @param propsStart Position of first byte of property data (after the length VBI).
     * @param propsEnd   Position one past the last byte of the properties section.
     * @param id         Property identifier to search for.
     * @param valueOut   Set to pointer to value bytes inside _buffer (may be nullptr).
     * @param valueLenOut Set to length of value (may be nullptr).
     * @return true if property was found.
     */
    bool findProperty(size_t propsStart, size_t propsEnd, uint8_t id,
                      const uint8_t** valueOut, uint16_t* valueLenOut);

   public:
    /**
     * @brief Creates an uninitialised client instance.
     * @note Before it can be used, it must be configured using the property setters setClient() and setServer().
     */
    PubSubClient();

    /**
     * @brief Creates a partially initialised client instance.
     * @param client The network client to use, for example WiFiClient.
     * @note Before it can be used, it must be configured with the property setter setServer().
     */
    PubSubClient(Client& client);

    /**
     * @brief Creates a fully configured client instance.
     * @param addr The address of the server.
     * @param port The port to connect to.
     * @param client The network client to use, for example WiFiClient.
     */
    PubSubClient(IPAddress addr, uint16_t port, Client& client);

    /**
     * @brief Creates a fully configured client instance.
     * @param addr The address of the server.
     * @param port The port to connect to.
     * @param client The network client to use, for example WiFiClient.
     * @param stream A stream to write received messages to.
     */
    PubSubClient(IPAddress addr, uint16_t port, Client& client, Stream& stream);

    /**
     * @brief Creates a fully configured client instance.
     * @param addr The address of the server.
     * @param port The port to connect to.
     * @param callback Pointer to a message \ref callback function.
     * Called when a message arrives for a subscription created by this client.
     * @param client The network client to use, for example WiFiClient.
     */
    PubSubClient(IPAddress addr, uint16_t port, MQTT_CALLBACK_SIGNATURE, Client& client);

    /**
     * @brief Creates a fully configured client instance.
     * @param addr The address of the server.
     * @param port The port to connect to.
     * @param callback Pointer to a message \ref callback function.
     * Called when a message arrives for a subscription created by this client.
     * @param client The network client to use, for example WiFiClient.
     * @param stream A stream to write received messages to.
     */
    PubSubClient(IPAddress addr, uint16_t port, MQTT_CALLBACK_SIGNATURE, Client& client, Stream& stream);

    /**
     * @brief Creates a fully configured client instance.
     * @param ip The address of the server.
     * @param port The port to connect to.
     * @param client The network client to use, for example WiFiClient.
     */
    PubSubClient(uint8_t* ip, uint16_t port, Client& client);

    /**
     * @brief Creates a fully configured client instance.
     * @param ip The address of the server.
     * @param port The port to connect to.
     * @param client The network client to use, for example WiFiClient.
     * @param stream A stream to write received messages to.
     */
    PubSubClient(uint8_t* ip, uint16_t port, Client& client, Stream& stream);

    /**
     * @brief Creates a fully configured client instance.
     * @param ip The address of the server.
     * @param port The port to connect to.
     * @param callback Pointer to a message \ref callback function.
     * Called when a message arrives for a subscription created by this client.
     * @param client The network client to use, for example WiFiClient.
     */
    PubSubClient(uint8_t* ip, uint16_t port, MQTT_CALLBACK_SIGNATURE, Client& client);

    /**
     * @brief Creates a fully configured client instance.
     * @param ip The address of the server.
     * @param port The port to connect to.
     * @param callback Pointer to a message \ref callback function.
     * Called when a message arrives for a subscription created by this client.
     * @param client The network client to use, for example WiFiClient.
     * @param stream A stream to write received messages to.
     */
    PubSubClient(uint8_t* ip, uint16_t port, MQTT_CALLBACK_SIGNATURE, Client& client, Stream& stream);

    /**
     * @brief Creates a fully configured client instance.
     * @param domain The address of the server.
     * @param port The port to connect to.
     * @param client The network client to use, for example WiFiClient.
     */
    PubSubClient(const char* domain, uint16_t port, Client& client);

    /**
     * @brief Creates a fully configured client instance.
     * @param domain The address of the server.
     * @param port The port to connect to.
     * @param client The network client to use, for example WiFiClient.
     * @param stream A stream to write received messages to.
     */
    PubSubClient(const char* domain, uint16_t port, Client& client, Stream& stream);

    /**
     * @brief Creates a fully configured client instance.
     * @param domain The address of the server.
     * @param port The port to connect to.
     * @param callback Pointer to a message \ref callback function.
     * Called when a message arrives for a subscription created by this client.
     * @param client The network client to use, for example WiFiClient.
     */
    PubSubClient(const char* domain, uint16_t port, MQTT_CALLBACK_SIGNATURE, Client& client);

    /**
     * @brief Creates a fully configured client instance.
     * @param domain The address of the server.
     * @param port The port to connect to.
     * @param callback Pointer to a message \ref callback function.
     * Called when a message arrives for a subscription created by this client.
     * @param client The network client to use, for example WiFiClient.
     * @param stream A stream to write received messages to.
     */
    PubSubClient(const char* domain, uint16_t port, MQTT_CALLBACK_SIGNATURE, Client& client, Stream& stream);

    /**
     * @brief Destructor for the PubSubClient class.
     */
    ~PubSubClient();

    /**
     * @brief Sets the MQTT protocol version to use at runtime.
     * Must be called before connect(). Does not affect an active connection.
     * @param version One of #MQTT_VERSION_3_1, #MQTT_VERSION_3_1_1, or #MQTT_VERSION_5.
     * @return The client instance, allowing the function to be chained.
     */
    PubSubClient& setMqttVersion(uint8_t version);

    /**
     * @brief Returns the currently configured MQTT protocol version.
     * @return One of #MQTT_VERSION_3_1, #MQTT_VERSION_3_1_1, or #MQTT_VERSION_5.
     */
    uint8_t getMqttVersion() const;

    /**
     * @brief Sets the server details.
     * @param ip The address of the server.
     * @param port The port to connect to.
     * @return The client instance, allowing the function to be chained.
     */
    PubSubClient& setServer(IPAddress ip, uint16_t port);

    /**
     * @brief Sets the server details.
     * @param ip The address of the server.
     * @param port The port to connect to.
     * @return The client instance, allowing the function to be chained.
     */
    PubSubClient& setServer(uint8_t* ip, uint16_t port);

    /**
     * @brief Sets the server details.
     * @param domain The address of the server.
     * @param port The port to connect to.
     * @return The client instance, allowing the function to be chained.
     */
    PubSubClient& setServer(const char* domain, uint16_t port);

    /**
     * @brief Sets the message callback function.
     * @param callback Pointer to a message \ref callback function.
     * Called when a message arrives for a subscription created by this client.
     * @return The client instance, allowing the function to be chained.
     */
    PubSubClient& setCallback(MQTT_CALLBACK_SIGNATURE);

    /**
     * @brief Sets the network client instance to use.
     * @param client The network client to use, for example WiFiClient.
     * @return The client instance, allowing the function to be chained.
     */
    PubSubClient& setClient(Client& client);

    /**
     * @brief Sets the stream to write received messages to.
     * @param stream A stream to write received messages to.
     * @return The client instance, allowing the function to be chained.
     */
    PubSubClient& setStream(Stream& stream);

    /**
     * @brief Sets the keep alive interval used by the client.
     * This value should only be changed when the client is not connected.
     * Set keepAlive to zero (0) to turn off the keep alive mechanism.
     * @param keepAlive The keep alive interval, in seconds.
     * @return The client instance, allowing the function to be chained.
     */
    PubSubClient& setKeepAlive(uint16_t keepAlive);

    /**
     * @brief Sets the socket timeout used by the client.
     * This determines how long the client will wait for incoming data when it expects data to arrive - for example,
     * whilst it is in the middle of reading a MQTT packet.
     * @param timeout The socket timeout, in seconds.
     * @return The client instance, allowing the function to be chained.
     */
    PubSubClient& setSocketTimeout(uint16_t timeout);

    /**
     * @brief Sets the size, in bytes, of the internal send and receive buffer.
     * This must be large enough to contain the full MQTT packet.
     * When sending or receiving messages, the packet will contain the full topic string,
     * the payload data, and a small number of header bytes.
     * @param size The size, in bytes, for the internal buffer.
     * @return true If the buffer was resized.
     * false If the buffer could not be resized.
     */
    bool setBufferSize(size_t size);

    /**
     * @brief Gets the current size of the internal buffer.
     * @return The size of the internal buffer.
     */
    size_t getBufferSize();

    /**
     * @brief Connects the client using a clean session without username and password.
     * @param id The client ID to use when connecting to the server.
     * @return true If client succeeded in establishing a connection to the broker.
     * false If client failed to establish a connection to the broker.
     */
    inline bool connect(const char* id) {
        return connect(id, nullptr, nullptr, nullptr, MQTT_QOS0, false, nullptr, true);
    }

    /**
     * @brief Connects the client using a clean session with username and password.
     * @param id The client ID to use when connecting to the server.
     * @param user The username to use.
     * @param pass The password to use.
     * @note If **user** is NULL, no username or password is used.
     * @note If **pass** is NULL, no password is used.
     * @return true If client succeeded in establishing a connection to the broker.
     * false If client failed to establish a connection to the broker.
     */
    inline bool connect(const char* id, const char* user, const char* pass) {
        return connect(id, user, pass, nullptr, MQTT_QOS0, false, nullptr, true);
    }

    /**
     * @brief Connects the client using a clean session and will.
     * @param id The client ID to use when connecting to the server.
     * @param willTopic The topic to be used by the will message.
     * @param willQos The quality of service to be used by the will message. [0, 1, 2].
     * @param willRetain Publish the will message with the retain flag.
     * @param willMessage The message to be used by the will message.
     * @note If **willTopic** is NULL, no will message is sent.
     * @return true If client succeeded in establishing a connection to the broker.
     * false If client failed to establish a connection to the broker.
     */
    inline bool connect(const char* id, const char* willTopic, uint8_t willQos, bool willRetain, const char* willMessage) {
        return connect(id, nullptr, nullptr, willTopic, willQos, willRetain, willMessage, true);
    }

    /**
     * @brief Connects the client using a clean session with username, password and will.
     * @param id The client ID to use when connecting to the server.
     * @param user The username to use.
     * @param pass The password to use.
     * @param willTopic The topic to be used by the will message.
     * @param willQos The quality of service to be used by the will message. [0, 1, 2].
     * @param willRetain Publish the will message with the retain flag.
     * @param willMessage The message to be used by the will message.
     * @note If **user** is NULL, no username or password is used.
     * @note If **pass** is NULL, no password is used.
     * @note If **willTopic** is NULL, no will message is sent.
     * @return true If client succeeded in establishing a connection to the broker.
     * false If client failed to establish a connection to the broker.
     */
    inline bool connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, bool willRetain,
                        const char* willMessage) {
        return connect(id, user, pass, willTopic, willQos, willRetain, willMessage, true);
    }

    /**
     * @brief Connects the client with all possible parameters (user, password, will and session).
     * @param id The client ID to use when connecting to the server.
     * @param user The username to use.
     * @param pass The password to use.
     * @param willTopic The topic to be used by the will message.
     * @param willQos The quality of service to be used by the will message. [0, 1, 2].
     * @param willRetain Publish the will message with the retain flag.
     * @param willMessage The message to be used by the will message.
     * @param cleanSession True to connect with a clean session.
     * @note If **user** is NULL, no username or password is used.
     * @note If **pass** is NULL, no password is used.
     * @note If **willTopic** is NULL, no will message is sent.
     * @return true If client succeeded in establishing a connection to the broker.
     * false If client failed to establish a connection to the broker.
     */
    bool connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, bool willRetain, const char* willMessage,
                 bool cleanSession);

    /**
     * @brief Disconnects the client.
     */
    void disconnect();

    /**
     * @brief Publishes a non retained message to the specified topic using QoS 0.
     * @param topic The topic to publish to.
     * @param payload The message to publish.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool publish(const char* topic, const char* payload) {
        return publish(topic, payload, MQTT_QOS0, false);
    }

    /**
     * @brief Publishes a message to the specified topic using QoS 0.
     * @param topic The topic to publish to.
     * @param payload The message to publish.
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool publish(const char* topic, const char* payload, bool retained) {
        return publish(topic, payload, MQTT_QOS0, retained);
    }

    /**
     * @brief Publishes a message to the specified topic.
     * @param topic The topic to publish to.
     * @param payload The message to publish.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool publish(const char* topic, const char* payload, uint8_t qos, bool retained) {
        return publish(topic, reinterpret_cast<const uint8_t*>(payload), payload ? strlen(payload) : 0, qos, retained);
    }

    /**
     * @brief Publishes a message to the specified topic.
     * @param topic The topic from __FlashStringHelper to publish to.
     * @param payload The message to publish.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool publish(const __FlashStringHelper* topic, const char* payload, uint8_t qos, bool retained) {
        return publish(topic, reinterpret_cast<const uint8_t*>(payload), payload ? strlen(payload) : 0, qos, retained);
    }

    /**
     * @brief Publishes a message from __FlashStringHelper to the specified topic from __FlashStringHelper.
     * @param topic The topic to publish to.
     * @param payload The message to publish.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool publish(const __FlashStringHelper* topic, const __FlashStringHelper* payload, uint8_t qos, bool retained) {
        return publish_P(topic, reinterpret_cast<const uint8_t*>(payload), payload ? strlen_P(reinterpret_cast<const char*>(payload)) : 0, qos, retained);
    }

    /**
     * @brief Publishes a non retained message to the specified topic using QoS 0.
     * @param topic The topic to publish to.
     * @param payload The message to publish.
     * @param plength The length of the payload.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool publish(const char* topic, const uint8_t* payload, size_t plength) {
        return publish(topic, payload, plength, MQTT_QOS0, false);
    }

    /**
     * @brief Publishes a message to the specified topic using QoS 0.
     * @param topic The topic to publish to.
     * @param payload The message to publish.
     * @param plength The length of the payload.
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool publish(const char* topic, const uint8_t* payload, size_t plength, bool retained) {
        return publish(topic, payload, plength, MQTT_QOS0, retained);
    }

    /**
     * @brief Publishes a message to the specified topic.
     * @param topic The topic to publish to.
     * @param payload The message to publish.
     * @param plength The length of the payload.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    bool publish(const char* topic, const uint8_t* payload, size_t plength, uint8_t qos, bool retained);

    /**
     * @brief Publishes a message to the specified topic.
     * @param topic The topic from __FlashStringHelper to publish to.
     * @param payload The message to publish.
     * @param plength The length of the payload.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    bool publish(const __FlashStringHelper* topic, const uint8_t* payload, size_t plength, uint8_t qos, bool retained);

    /**
     * @brief Publishes a message stored in PROGMEM to the specified topic using QoS 0.
     * @param topic The topic to publish to.
     * @param payload The message to publish.
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool publish_P(const char* topic, PGM_P payload, bool retained) {
        return publish_P(topic, payload, MQTT_QOS0, retained);
    }

    /**
     * @brief Publishes a message stored in PROGMEM to the specified topic.
     * @param topic The topic to publish to.
     * @param payload The message to publish.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool publish_P(const char* topic, PGM_P payload, uint8_t qos, bool retained) {
        return publish_P(topic, reinterpret_cast<const uint8_t*>(payload), payload ? strlen_P(payload) : 0, qos, retained);
    }

    /**
     * @brief Publishes a message stored in PROGMEM to the specified topic.
     * @param topic The topic from __FlashStringHelper to publish to.
     * @param payload The message to publish.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    bool publish_P(const __FlashStringHelper* topic, PGM_P payload, uint8_t qos, bool retained) {
        return publish_P(topic, reinterpret_cast<const uint8_t*>(payload), payload ? strlen_P(payload) : 0, qos, retained);
    }

    /**
     * @brief Publishes a message stored in PROGMEM to the specified topic using QoS 0.
     * @param topic The topic to publish to.
     * @param payload The message to publish.
     * @param plength The length of the payload.
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool publish_P(const char* topic, const uint8_t* payload, size_t plength, bool retained) {
        return publish_P(topic, payload, plength, MQTT_QOS0, retained);
    }

    /**
     * @brief Publishes a message stored in PROGMEM to the specified topic.
     * @param topic The topic to publish to.
     * @param payload The message to publish.
     * @param plength The length of the payload.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    bool publish_P(const char* topic, const uint8_t* payload, size_t plength, uint8_t qos, bool retained);

    /**
     * @brief Publishes a message stored in PROGMEM to the specified topic.
     * @param topic The topic from __FlashStringHelper to publish to.
     * @param payload The message from PROGMEM to publish.
     * @param plength The length of the payload.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    bool publish_P(const __FlashStringHelper* topic, const uint8_t* payload, size_t plength, uint8_t qos, bool retained);

    /**
     * @brief Start to publish a message using QoS 0.
     * This API:
     *   beginPublish(...)
     *   one or more calls to write(...)
     *   endPublish()
     * Allows for arbitrarily large payloads to be sent without them having to be copied into
     * a new buffer and held in memory at one time.
     * @param topic The topic to publish to.
     * @param plength The length of the payload.
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool beginPublish(const char* topic, size_t plength, bool retained) {
        return beginPublishImpl(false, topic, plength, MQTT_QOS0, retained);
    }

    /**
     * @brief Start to publish a message.
     * This API:
     *   beginPublish(...)
     *   one or more calls to write(...)
     *   endPublish()
     * Allows for arbitrarily large payloads to be sent without them having to be copied into
     * a new buffer and held in memory at one time.
     * @param topic The topic to publish to.
     * @param plength The length of the payload.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool beginPublish(const char* topic, size_t plength, uint8_t qos, bool retained) {
        return beginPublishImpl(false, topic, plength, qos, retained);
    }

    /**
     * @brief Start to publish a message using a topic from __FlashStringHelper F().
     * This API:
     *   beginPublish(...)
     *   one or more calls to write(...)
     *   endPublish()
     * Allows for arbitrarily large payloads to be sent without them having to be copied into
     * a new buffer and held in memory at one time.
     * @param topic The topic from __FlashStringHelper to publish to.
     * @param plength The length of the payload.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool beginPublish(const __FlashStringHelper* topic, size_t plength, uint8_t qos, bool retained) {
        // convert FlashStringHelper in PROGMEM-pointer
        return beginPublishImpl(true, reinterpret_cast<const char*>(topic), plength, qos, retained);
    }

    /**
     * @brief Start to publish a message using a topic in PROGMEM.
     * This API:
     *   beginPublish_P(...)
     *   one or more calls to write(...)
     *   endPublish()
     * Allows for arbitrarily large payloads to be sent without them having to be copied into
     * a new buffer and held in memory at one time.
     * @param topic The topic in PROGMEM to publish to.
     * @param plength The length of the payload.
     * @param qos The quality of service (\ref group_qos) to publish at. [0, 1, 2].
     * @param retained Publish the message with the retain flag.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    inline bool beginPublish_P(PGM_P topic, size_t plength, uint8_t qos, bool retained) {
        return beginPublishImpl(true, reinterpret_cast<const char*>(topic), plength, qos, retained);
    }

    /**
     * @brief Finish sending a message that was started with a call to beginPublish.
     * @return true If the publish succeeded.
     * false If the publish failed, either connection lost or message too large.
     */
    bool endPublish();

    /**
     * @brief Writes a single byte as a component of a publish started with a call to beginPublish.
     *        For performance reasons, this will be appended to the internal buffer,
     *        which will be flushed when full or on a call to endPublish().
     * @param data A byte to write to the publish payload.
     * @return The number of bytes written (0 or 1). If 0 is returned a write error occurred.
     */
    virtual size_t write(uint8_t data);

    /**
     * @brief Writes an array of bytes as a component of a publish started with a call to beginPublish.
     *        For performance reasons, this will be appended to the internal buffer,
     *        which will be flushed when full or on a call to endPublish().
     * @param buf The bytes to write.
     * @param size The length of the payload to be sent.
     * @return The number of bytes written. If return value is != size a write error occurred.
     */
    virtual size_t write(const uint8_t* buf, size_t size);

    /**
     * @brief Writes a string in PROGMEM as a component of a publish started with a call to beginPublish.
     *        For performance reasons, this will be appended to the internal buffer,
     *        which will be flushed when full or on a call to endPublish().
     * @param string The message to write.
     * @return The number of bytes written. If return value is != string length a write error occurred.
     */
    inline size_t write_P(PGM_P string) {
        return write_P(reinterpret_cast<const uint8_t*>(string), strlen_P(string));
    }

    /**
     * @brief Writes an array of progmem bytes as a component of a publish started with a call to beginPublish.
     *        For performance reasons, this will be appended to the internal buffer,
     *        which will be flushed when full or on a call to endPublish().
     * @param buf The bytes to write.
     * @param size The length of the payload to be sent.
     * @return The number of bytes written. If return value is != size a write error occurred.
     */
    size_t write_P(const uint8_t* buf, size_t size);

    /**
     * @brief Subscribes to messages published to the specified topic using QoS 0.
     * @param topic The topic to subscribe to.
     * @return true If sending the subscribe succeeded.
     * false If sending the subscribe failed, either connection lost or message too large.
     */
    inline bool subscribe(const char* topic) {
        return subscribeImpl(false, topic, MQTT_QOS0);
    }

    /**
     * @brief Subscribes to messages published to the specified topic from __FlashStringHelper using QoS 0.
     * @param topic The topic from __FlashStringHelper to subscribe to.
     * @return true If sending the subscribe succeeded.
     * false If sending the subscribe failed, either connection lost or message too large.
     */
    inline bool subscribe(const __FlashStringHelper* topic) {
        // convert FlashStringHelper in PROGMEM-pointer
        return subscribeImpl(true, reinterpret_cast<const char*>(topic), MQTT_QOS0);
    }

    /**
     * @brief Subscribes to messages published to the specified topic in PROGMEM using QoS 0.
     * @param topic The topic in PROGMEM to subscribe to.
     * @return true If sending the subscribe succeeded.
     * false If sending the subscribe failed, either connection lost or message too large.
     */
    inline bool subscribe_P(PGM_P topic) {
        return subscribeImpl(true, reinterpret_cast<const char*>(topic), MQTT_QOS0);
    }

    /**
     * @brief Subscribes to messages published to the specified topic.
     * @param topic The topic to subscribe to.
     * @param qos The qos to subscribe at. [0, 1].
     * @return true If sending the subscribe succeeded.
     * false If sending the subscribe failed, either connection lost or message too large.
     */
    inline bool subscribe(const char* topic, uint8_t qos) {
        return subscribeImpl(false, topic, qos);
    }

    /**
     * @brief Subscribes to messages published to the specified topic from __FlashStringHelper.
     * @param topic The topic from __FlashStringHelper to subscribe to.
     * @param qos The qos to subscribe at. [0, 1].
     * @return true If sending the subscribe succeeded.
     * false If sending the subscribe failed, either connection lost or message too large.
     */
    inline bool subscribe(const __FlashStringHelper* topic, uint8_t qos) {
        // convert FlashStringHelper in PROGMEM-pointer
        return subscribeImpl(true, reinterpret_cast<const char*>(topic), qos);
    }

    /**
     * @brief Subscribes to messages published to the specified topic in PROGMEM.
     * @param topic The topic in PROGMEM to subscribe to.
     * @param qos The qos to subscribe at. [0, 1].
     * @return true If sending the subscribe succeeded.
     * false If sending the subscribe failed, either connection lost or message too large.
     */
    inline bool subscribe_P(PGM_P topic, uint8_t qos) {
        return subscribeImpl(true, reinterpret_cast<const char*>(topic), qos);
    }

    /**
     * @brief Unsubscribes from the specified topic.
     * @param topic The topic to unsubscribe from.
     * @return true If sending the unsubscribe succeeded.
     * false If sending the unsubscribe failed, either connection lost or message too large.
     */
    inline bool unsubscribe(const char* topic) {
        return unsubscribeImpl(false, topic);
    }

    /**
     * @brief Unsubscribes from the specified topic from __FlashStringHelper.
     * @param topic The topic from __FlashStringHelper to unsubscribe from.
     * @return true If sending the unsubscribe succeeded.
     * false If sending the unsubscribe failed, either connection lost or message too large.
     */
    inline bool unsubscribe(const __FlashStringHelper* topic) {
        // convert FlashStringHelper in PROGMEM-pointer
        return unsubscribeImpl(true, reinterpret_cast<const char*>(topic));
    }

    /**
     * @brief Unsubscribes from the specified topic in PROGMEM.
     * @param topic The topic in PROGMEM to unsubscribe from.
     * @return true If sending the unsubscribe succeeded.
     * false If sending the unsubscribe failed, either connection lost or message too large.
     */
    inline bool unsubscribe_P(PGM_P topic) {
        return unsubscribeImpl(true, reinterpret_cast<const char*>(topic));
    }

    /**
     * @brief This should be called regularly to allow the client to process incoming messages and maintain its connection to the server.
     * @return true If the client is still connected.
     * false If the client is no longer connected.
     */
    bool loop();

    /**
     * @brief Checks whether the client is connected to the server.
     * @return true If the client is connected.
     * false If the client is not connected.
     */
    bool connected();

    /**
     * @brief Returns the current state of the client.
     * If a connection attempt fails, this can be used to get more information about the failure.
     * @note All of the values have corresponding constants defined in PubSubClient.h.
     * @return See \ref group_state
     */
    int state();
};

#endif
