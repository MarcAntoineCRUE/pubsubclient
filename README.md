# Arduino Client for MQTT

This library provides a client for doing simple publish/subscribe messaging with
a server that supports MQTT.

## Notes of this fork

This is a fork of the repository [knolleary/pubsubclient v2.8](https://github.com/knolleary/pubsubclient/releases/tag/v2.8), which was last updated in May 20, 2020. There was an update approach in [#1045](https://github.com/knolleary/pubsubclient/issues/1045), but it's also stale.

I tried lot's of different other MQTT libs, but they need more resources than PubSubClient or lacking maintenance as well:

- <https://github.com/256dpi/arduino-mqtt>
- <https://github.com/hideakitai/MQTTPubSubClient>
- <https://github.com/bertmelis/espMqttClient>
- <https://github.com/arduino-libraries/ArduinoMqttClient>
- <https://github.com/thingsboard/pubsubclient>

Since there was no progress I decided to merge the most important PRs manually and publish a new major version. I also renamed to PubSubClient3 to have a similar but different name of the library.

I appreciate every contribution to this library.

## Examples

The library comes with a number of example sketches. See File > Examples > PubSubClient
within the Arduino application.

Full API documentation is available here: <https://hmueller01.github.io/pubsubclient3/api>

## Limitations

- The client is based on the [MQTT Version 3.1.1 specification](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html) with the following limitations.

### QoS support

| Feature | QoS 0 | QoS 1 | QoS 2 |
|---------|-------|-------|-------|
| Publish | ✅ | ✅ | ✅ |
| Subscribe | ✅ | ✅ | ✅ |
| Inbound retransmission (DUP) | n/a | ignored¹ | ✅ deduplicated |
| Outbound retransmission | n/a | ❌ | PUBREL only² |

¹ QoS 1 inbound duplicate PUBLISH packets (DUP flag) are not detected and will invoke the callback again.

² For outbound QoS 2: the initial PUBLISH is **not** retransmitted if PUBREC is not received (TCP is expected to ensure delivery). However, PUBREL **is** retransmitted if PUBCOMP is not received within `MQTT_QOS2_RETRY_TIMEOUT` seconds, as required by MQTT 3.1.1 §4.4. For QoS 1 outbound, no retransmission is performed either. Since MQTT runs over TCP, retransmission is not required in the vast majority of cases.

When publishing at QoS 2, use `isPublishQoS2Complete()` to wait for the 4-step handshake (PUBLISH → PUBREC → PUBREL → PUBCOMP) to finish before publishing the next QoS 2 message.

### Other limitations

- The maximum message size, including header, is **256 bytes** by default. This
   is configurable via `MQTT_MAX_PACKET_SIZE` in `PubSubClient.h` or can be changed
   at runtime by calling `PubSubClient::setBufferSize(size)`.
- The keepalive interval is set to 15 seconds by default. This is configurable
   via `MQTT_KEEPALIVE` in `PubSubClient.h` or can be changed at runtime by calling
   `PubSubClient::setKeepAlive(keepAlive)`.
- The client uses MQTT 3.1.1 by default. It can be changed to use MQTT 3.1 by
   setting `MQTT_VERSION` to `MQTT_VERSION_3_1` in `PubSubClient.h`.
- Since [v3.3.0](https://github.com/hmueller01/pubsubclient3/releases/tag/v3.3.0) it can publish and subscribe to `PROGMEM` or `__FlashStringHelper` topics.
   Details see the [mqtt_progmem](https://github.com/hmueller01/pubsubclient3/blob/aae84e4d1aa65e752e19e30239b5796b4fe2705b/examples/mqtt_progmem/src/mqtt_progmem.cpp#L39-L48) example.
   Note that `client.publish_P(...)` is reserved for PROGMEM **payloads**; to publish a PROGMEM **topic** use:

   ```c
   const char TOPIC[] PROGMEM = "test";
   const char HELLO_WORLD[] PROGMEM = "hello world";
   client.beginPublish_P(TOPIC, strlen_P(HELLO_WORLD), MQTT_QOS0, false);
   client.write_P(HELLO_WORLD);
   client.endPublish();
   ```

## Configuration

The following macros can be defined **before** including `PubSubClient.h` (or in `PubSubClient.h` directly) to configure the library:

| Macro | Default | Description |
|-------|---------|-------------|
| `MQTT_VERSION` | `MQTT_VERSION_3_1_1` | MQTT protocol version. Use `MQTT_VERSION_3_1` for MQTT 3.1. |
| `MQTT_MAX_PACKET_SIZE` | `256` | Largest packet size (bytes) the client will send or receive. Packets exceeding this size are dropped. Can also be set at runtime with `setBufferSize()`. |
| `MQTT_KEEPALIVE` | `15` | Keepalive interval in seconds. Set to `0` to disable. Can also be set at runtime with `setKeepAlive()`. |
| `MQTT_SOCKET_TIMEOUT` | `15` | Network read timeout in seconds. Also applies to `connect()`. Can also be set at runtime with `setSocketTimeout()`. |
| `MQTT_QOS2_RETRY_TIMEOUT` | `10` | Seconds before PUBREL is retransmitted when awaiting PUBCOMP (QoS 2 outbound). |
| `MQTT_MAX_TRANSFER_SIZE` | *(undefined)* | Maximum bytes per network write call. Useful for hardware with small write buffers (e.g. Arduino WiFi Shield: use `80`). Undefined by default (sends entire packet in one call). |
| `NOFUNCTIONAL` | *(undefined)* | Define to replace `std::function` with a raw function pointer for the message callback. Saves ~12–16 bytes of RAM, but lambdas with captures are no longer accepted. |
| `DEBUG_PUBSUBCLIENT` | *(undefined)* | Define together with `DEBUG_ESP_PORT` to enable verbose debug output from the library via `DEBUG_ESP_PORT.printf(...)`. |

## API Reference

Full Doxygen API documentation is available at: <https://hmueller01.github.io/pubsubclient3/api>

### Connection

```cpp
// Connect using only a client ID (clean session, no credentials)
bool connect(const char* id);

// Connect with credentials
bool connect(const char* id, const char* user, const char* pass);

// Connect with Last Will and Testament
bool connect(const char* id, const char* willTopic, uint8_t willQos,
             bool willRetain, const char* willMessage);

// Connect with credentials and Last Will and Testament
bool connect(const char* id, const char* user, const char* pass,
             const char* willTopic, uint8_t willQos, bool willRetain,
             const char* willMessage);

// Full connect (all parameters including clean session flag)
bool connect(const char* id, const char* user, const char* pass,
             const char* willTopic, uint8_t willQos, bool willRetain,
             const char* willMessage, bool cleanSession);

void disconnect();
bool connected();
bool loop();   // Must be called regularly to process incoming messages and keepalive
int  state();  // Returns one of the MQTT_* state constants
```

### Publish

```cpp
// Simple publish (QoS 0, not retained)
bool publish(const char* topic, const char* payload);
bool publish(const char* topic, const uint8_t* payload, size_t plength);

// Publish with retain flag (QoS 0)
bool publish(const char* topic, const char* payload, bool retained);

// Publish with QoS and retain flag
bool publish(const char* topic, const char* payload, uint8_t qos, bool retained);
bool publish(const char* topic, const uint8_t* payload, size_t plength,
             uint8_t qos, bool retained);

// Publish with PROGMEM payload
bool publish_P(const char* topic, PGM_P payload, bool retained);
bool publish_P(const char* topic, PGM_P payload, uint8_t qos, bool retained);
bool publish_P(const char* topic, const uint8_t* payload, size_t plength,
               uint8_t qos, bool retained);

// Streaming API for large payloads (avoids copying the full payload into RAM)
bool beginPublish(const char* topic, size_t plength, bool retained);
bool beginPublish(const char* topic, size_t plength, uint8_t qos, bool retained);
bool beginPublish_P(PGM_P topic, size_t plength, uint8_t qos, bool retained);
size_t write(uint8_t data);
size_t write(const uint8_t* buf, size_t size);
size_t write_P(PGM_P string);
size_t write_P(const uint8_t* buf, size_t size);
bool endPublish();

// QoS 2 handshake completion check
bool isPublishQoS2Complete();
```

QoS levels are defined as `MQTT_QOS0` (0), `MQTT_QOS1` (1), `MQTT_QOS2` (2).

### Subscribe / Unsubscribe

```cpp
bool subscribe(const char* topic);                         // QoS 0
bool subscribe(const char* topic, uint8_t qos);           // QoS 0, 1 or 2
bool subscribe_P(PGM_P topic);                            // PROGMEM topic, QoS 0
bool subscribe_P(PGM_P topic, uint8_t qos);               // PROGMEM topic

bool unsubscribe(const char* topic);
bool unsubscribe_P(PGM_P topic);                          // PROGMEM topic
```

Incoming messages are delivered to the callback registered via `setCallback()`:

```cpp
void callback(char* topic, uint8_t* payload, size_t plength) {
    // handle message
}
client.setCallback(callback);
```

### Configuration setters (chainable)

```cpp
PubSubClient& setServer(IPAddress ip, uint16_t port);
PubSubClient& setServer(const char* domain, uint16_t port);
PubSubClient& setCallback(callback_fn);
PubSubClient& setClient(Client& client);
PubSubClient& setStream(Stream& stream);
PubSubClient& setKeepAlive(uint16_t keepAlive);
PubSubClient& setSocketTimeout(uint16_t timeout);
bool          setBufferSize(size_t size);
size_t        getBufferSize();
```

### State constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `MQTT_CONNECTION_TIMEOUT` | -4 | Server did not respond within the keepalive window |
| `MQTT_CONNECTION_LOST` | -3 | Network connection was dropped |
| `MQTT_CONNECT_FAILED` | -2 | Network connection could not be established |
| `MQTT_DISCONNECTED` | -1 | Client disconnected cleanly |
| `MQTT_CONNECTED` | 0 | Client is connected |
| `MQTT_CONNECT_BAD_PROTOCOL` | 1 | Broker rejected the MQTT version |
| `MQTT_CONNECT_BAD_CLIENT_ID` | 2 | Broker rejected the client ID |
| `MQTT_CONNECT_UNAVAILABLE` | 3 | Broker temporarily unavailable |
| `MQTT_CONNECT_BAD_CREDENTIALS` | 4 | Invalid username or password |
| `MQTT_CONNECT_UNAUTHORIZED` | 5 | Client not authorized |

## Compatible Hardware

The library uses the Arduino Ethernet Client api for interacting with the
underlying network hardware. This means it Just Works with a growing number of
boards and shields, including:

- Arduino Ethernet
- Arduino Ethernet Shield
- Arduino YUN – use the included `YunClient` in place of `EthernetClient`, and
   be sure to do a `Bridge.begin()` first
- Arduino WiFi Shield - if you want to send packets > 90 bytes with this shield,
   enable the `MQTT_MAX_TRANSFER_SIZE` define in `PubSubClient.h`.
- Sparkfun WiFly Shield – [library](https://github.com/dpslwk/WiFly)
- TI CC3000 WiFi - [library](https://github.com/sparkfun/SFE_CC3000_Library)
- Intel Galileo/Edison
- ESP8266
- ESP32

The library cannot currently be used with hardware based on the ENC28J60 chip –
such as the Nanode or the Nuelectronics Ethernet Shield. For those, there is an
[alternative library](https://github.com/njh/NanodeMQTT) available.

## Changelog

See [CHANGELOG.md](CHANGELOG.md)

## License

This code is released under the [MIT License](LICENSE.txt).
