# Arduino Client for MQTT

This library provides a client for doing simple publish/subscribe messaging with
a server that supports MQTT.

## Examples

The library comes with a number of example sketches. See File > Examples > PubSubClient
within the Arduino application.

Full API documentation is available here: <https://pubsubclient.knolleary.net>

## Features & Limitations

- **QoS Support:**
  - Publish: Supports QoS 0, QoS 1, and QoS 2 messages.
  - Subscribe: Supports QoS 0, QoS 1, and QoS 2 subscriptions.
- **Protocol:**
  - MQTT 3.1.1 by default. MQTT 3.1 also supported (set `MQTT_VERSION` in `PubSubClient.h`).
  - MQTT 5 support is planned (see roadmap).
- **Message Size:**
  - The maximum message size, including header, is **256 bytes** by default. This is configurable via `MQTT_MAX_PACKET_SIZE` in `PubSubClient.h` or can be changed by calling `PubSubClient::setBufferSize(size)`.
- **Keepalive:**
  - The keepalive interval is set to 15 seconds by default. This is configurable via `MQTT_KEEPALIVE` in `PubSubClient.h` or can be changed by calling `PubSubClient::setKeepAlive(keepAlive)`.
- **Reliability:**
  - Robust handling of buffer overflows, invalid QoS, and protocol errors.
  - Duplicate suppression for QoS 2 as per MQTT spec.

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

## License

This code is released under the MIT License.
