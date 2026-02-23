/*

  PubSubClient.cpp - A simple client for MQTT.
  Nick O'Leary
  http://knolleary.net
*/

#include "PubSubClient.h"
#include "Arduino.h"

// -------------------------------------------------------
// Common initialisation — called by every constructor.
// Avoids ~300 lines of duplicated setup code.
// -------------------------------------------------------
void PubSubClient::init() {
    this->_state = MQTT_DISCONNECTED;
    this->_client = NULL;
    this->domain  = NULL;
    this->port    = 0;
    this->stream  = NULL;
    this->pingOutstanding = false;
    this->nextMsgId = 1;
    this->lastOutActivity = 0;
    this->lastInActivity  = 0;
    setCallback(NULL);
    this->bufferSize = 0;
    setBufferSize(MQTT_MAX_PACKET_SIZE);
    setKeepAlive(MQTT_KEEPALIVE);
    setSocketTimeout(MQTT_SOCKET_TIMEOUT);
    clearPendingMessages();
#if MQTT_ENABLE_V5
    this->_mqttVersion = MQTT_VERSION; // can be overridden with setMqttVersion()
    this->_v5ServerReceiveMax = 65535; // MQTT 5 spec default
    this->_v5ServerKeepalive  = 0;     // 0 = unset (use client keepAlive)
#endif
}

PubSubClient::PubSubClient() { init(); }

PubSubClient::PubSubClient(Client& client)
    { init(); setClient(client); }

PubSubClient::PubSubClient(IPAddress addr, uint16_t p, Client& client)
    { init(); setServer(addr,p); setClient(client); }

PubSubClient::PubSubClient(IPAddress addr, uint16_t p, Client& client, Stream& st)
    { init(); setServer(addr,p); setClient(client); setStream(st); }

PubSubClient::PubSubClient(IPAddress addr, uint16_t p, MQTT_CALLBACK_SIGNATURE, Client& client)
    { init(); setServer(addr,p); setCallback(callback); setClient(client); }

PubSubClient::PubSubClient(IPAddress addr, uint16_t p, MQTT_CALLBACK_SIGNATURE, Client& client, Stream& st)
    { init(); setServer(addr,p); setCallback(callback); setClient(client); setStream(st); }

PubSubClient::PubSubClient(uint8_t *ip, uint16_t p, Client& client)
    { init(); setServer(ip,p); setClient(client); }

PubSubClient::PubSubClient(uint8_t *ip, uint16_t p, Client& client, Stream& st)
    { init(); setServer(ip,p); setClient(client); setStream(st); }

PubSubClient::PubSubClient(uint8_t *ip, uint16_t p, MQTT_CALLBACK_SIGNATURE, Client& client)
    { init(); setServer(ip,p); setCallback(callback); setClient(client); }

PubSubClient::PubSubClient(uint8_t *ip, uint16_t p, MQTT_CALLBACK_SIGNATURE, Client& client, Stream& st)
    { init(); setServer(ip,p); setCallback(callback); setClient(client); setStream(st); }

PubSubClient::PubSubClient(const char* domain, uint16_t p, Client& client)
    { init(); setServer(domain,p); setClient(client); }

PubSubClient::PubSubClient(const char* domain, uint16_t p, Client& client, Stream& st)
    { init(); setServer(domain,p); setClient(client); setStream(st); }

PubSubClient::PubSubClient(const char* domain, uint16_t p, MQTT_CALLBACK_SIGNATURE, Client& client)
    { init(); setServer(domain,p); setCallback(callback); setClient(client); }

PubSubClient::PubSubClient(const char* domain, uint16_t p, MQTT_CALLBACK_SIGNATURE, Client& client, Stream& st)
    { init(); setServer(domain,p); setCallback(callback); setClient(client); setStream(st); }

PubSubClient::~PubSubClient() {
    free(this->buffer);
}

boolean PubSubClient::connect(const char *id) {
    return connect(id,NULL,NULL,0,0,0,0,1);
}

boolean PubSubClient::connect(const char *id, const char *user, const char *pass) {
    return connect(id,user,pass,0,0,0,0,1);
}

boolean PubSubClient::connect(const char *id, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage) {
    return connect(id,NULL,NULL,willTopic,willQos,willRetain,willMessage,1);
}

boolean PubSubClient::connect(const char *id, const char *user, const char *pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage) {
    return connect(id,user,pass,willTopic,willQos,willRetain,willMessage,1);
}

boolean PubSubClient::connect(const char *id, const char *user, const char *pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage, boolean cleanSession) {
    // Validate arguments before touching the socket
    if (willTopic && willQos > 2) return false;

    if (!connected()) {
        int result = 0;


        if(_client->connected()) {
            result = 1;
        } else {
            if (domain != NULL) {
                result = _client->connect(this->domain, this->port);
            } else {
                result = _client->connect(this->ip, this->port);
            }
        }

        if (result == 1) {
            nextMsgId = 1;
            clearPendingMessages();
            // Leave room in the buffer for header and variable length field
            uint16_t length = MQTT_MAX_HEADER_SIZE;
            unsigned int j;

#if MQTT_ENABLE_V5
            if (_mqttVersion == MQTT_VERSION_5) {
                // MQTT 5 protocol name + level
                uint8_t d[7] = {0x00,0x04,'M','Q','T','T', 0x05};
                for (uint8_t k = 0; k < 7; k++) this->buffer[length++] = d[k];
            } else
#endif
            {
#if MQTT_VERSION == MQTT_VERSION_3_1
                uint8_t d[9] = {0x00,0x06,'M','Q','I','s','d','p', MQTT_VERSION};
                for (uint8_t k = 0; k < 9; k++) this->buffer[length++] = d[k];
#else
                uint8_t d[7] = {0x00,0x04,'M','Q','T','T', MQTT_VERSION};
                for (uint8_t k = 0; k < 7; k++) this->buffer[length++] = d[k];
#endif
            }

            uint8_t v;
            if (willTopic) {
                v = 0x04|(willQos<<3)|(willRetain<<5);
            } else {
                v = 0x00;
            }
            if (cleanSession) {
                v = v|0x02;
            }

            if(user != NULL) {
                v = v|0x80;

                if(pass != NULL) {
                    v = v|(0x80>>1);
                }
            }
            this->buffer[length++] = v;

            this->buffer[length++] = ((this->keepAlive) >> 8);
            this->buffer[length++] = ((this->keepAlive) & 0xFF);

#if MQTT_ENABLE_V5
            if (_mqttVersion == MQTT_VERSION_5) {
                // Connect Properties (empty — expanded in later steps)
                this->buffer[length++] = 0x00; // Property Length = 0
            }
#endif

            CHECK_STRING_LENGTH(length,id)
            length = writeString(id,this->buffer,length);
            if (willTopic) {
#if MQTT_ENABLE_V5
                if (_mqttVersion == MQTT_VERSION_5) {
                    // Will Properties (empty — expanded in later steps)
                    this->buffer[length++] = 0x00; // Will Property Length = 0
                }
#endif
                CHECK_STRING_LENGTH(length,willTopic)
                length = writeString(willTopic,this->buffer,length);
                CHECK_STRING_LENGTH(length,willMessage)
                // In MQTT 5 the Will Payload is binary (same 2-byte length prefix as UTF-8 string)
                length = writeString(willMessage,this->buffer,length);
            }

            if(user != NULL) {
                CHECK_STRING_LENGTH(length,user)
                length = writeString(user,this->buffer,length);
                if(pass != NULL) {
                    CHECK_STRING_LENGTH(length,pass)
                    length = writeString(pass,this->buffer,length);
                }
            }

            write(MQTTCONNECT,this->buffer,length-MQTT_MAX_HEADER_SIZE);

            lastInActivity = lastOutActivity = millis();

            while (!_client->available()) {
                unsigned long t = millis();
                // Cast to uint32_t to avoid overflow when socketTimeout is large
                if ((uint32_t)(t - lastInActivity) >= (uint32_t)this->socketTimeout * 1000UL) {
                    _state = MQTT_CONNECTION_TIMEOUT;
                    _client->stop();
                    return false;
                }
            }
            uint8_t llen;
            uint32_t len = readPacket(&llen);

#if MQTT_ENABLE_V5
            if (_mqttVersion == MQTT_VERSION_5) {
                // MQTT 5 CONNACK: [type][remLenVBI][sessionPresent][reasonCode][propsVBI...]
                // llen = number of VBI bytes for remaining length (typically 1)
                // Minimum: 5 bytes total (type + 1VBI + session_present + reason + 1props_VBI)
                uint16_t minLen = (uint16_t)(llen + 4); // type(1)+VBI(llen)+session(1)+reason(1)+propsLen(1)
                if (len >= minLen && buffer[llen + 2] == 0x00) {
                    // Success — parse properties to extract Server Keep Alive & Receive Maximum
                    uint16_t propsVBIStart = (uint16_t)(llen + 3);
                    if (propsVBIStart < len) {
                        uint8_t vbiBytes;
                        uint32_t propsLen = decodeVariableByteInteger(buffer + propsVBIStart, &vbiBytes);
                        uint16_t propsPStart = propsVBIStart + vbiBytes;
                        uint16_t propsPEnd   = propsPStart + (uint16_t)propsLen;
                        if (propsPEnd <= len) {
                            const uint8_t* propVal;
                            uint16_t propValLen;
                            if (findProperty(buffer, propsPStart, propsPEnd,
                                             MQTT_PROP_SERVER_KEEP_ALIVE, &propVal, &propValLen)) {
                                uint16_t ska = ((uint16_t)propVal[0] << 8) | propVal[1];
                                this->keepAlive = ska;
                                this->_v5ServerKeepalive = ska;
                            }
                            if (findProperty(buffer, propsPStart, propsPEnd,
                                             MQTT_PROP_RECEIVE_MAXIMUM, &propVal, &propValLen)) {
                                uint16_t rm = ((uint16_t)propVal[0] << 8) | propVal[1];
                                this->_v5ServerReceiveMax = rm;
                            }
                        }
                    }
                    lastInActivity = millis();
                    pingOutstanding = false;
                    _state = MQTT_CONNECTED;
                    return true;
                } else if (len >= minLen) {
                    _state = (int8_t)buffer[llen + 2]; // store reason code
                }
            } else
#endif
            if (len == 4) {
                if (buffer[3] == 0) {
                    lastInActivity = millis();
                    pingOutstanding = false;
                    _state = MQTT_CONNECTED;
                    return true;
                } else {
                    _state = buffer[3];
                }
            }
            _client->stop();
        } else {
            _state = MQTT_CONNECT_FAILED;
        }
        return false;
    }
    return true;
}

// reads a byte into result
boolean PubSubClient::readByte(uint8_t * result) {
   uint32_t previousMillis = millis();
   while(!_client->available()) {
     yield();
     uint32_t currentMillis = millis();
     if((uint32_t)(currentMillis - previousMillis) >= (uint32_t)this->socketTimeout * 1000UL){
       return false;
     }
   }
   *result = _client->read();
   return true;
}

// reads a byte into result[*index] and increments index
boolean PubSubClient::readByte(uint8_t * result, uint16_t * index){
  uint16_t current_index = *index;
  uint8_t * write_address = &(result[current_index]);
  if(readByte(write_address)){
    *index = current_index + 1;
    return true;
  }
  return false;
}

// Bulk-reads 'count' bytes into buf.
// Significantly faster than byte-by-byte for large payloads.
boolean PubSubClient::readBytes(uint8_t* buf, uint16_t count) {
    uint32_t startMs = millis();
    uint16_t got = 0;
    while (got < count) {
        int avail = _client->available();
        if (avail > 0) {
            uint16_t chunk = (avail < (count - got)) ? (uint16_t)avail : (count - got);
            int n = _client->read(buf + got, chunk);
            if (n > 0) got += (uint16_t)n;
        } else if (!_client->connected()) {
            return false;
        } else {
            yield();
            if ((uint32_t)(millis() - startMs) >= (uint32_t)this->socketTimeout * 1000UL) {
                return false;
            }
        }
    }
    return true;
}

uint32_t PubSubClient::readPacket(uint8_t* lengthLength) {
    uint16_t len = 0;
    if(!readByte(this->buffer, &len)) return 0;
    bool isPublish = (this->buffer[0]&0xF0) == MQTTPUBLISH;
    uint8_t digit = 0;
    uint16_t skip = 0;
    uint32_t start = 0;

    // Read remaining length (Variable Byte Integer) into buffer
    do {
        if (len == 5) {
            // Invalid remaining length encoding — kill the connection
            _state = MQTT_DISCONNECTED;
            _client->stop();
            return 0;
        }
        if (!readByte(&digit)) return 0;
        this->buffer[len++] = digit;
    } while ((digit & 0x80) != 0);
    // Decode the VBI from the buffer bytes that were just read
    uint8_t vbiBytes;
    uint32_t length = decodeVariableByteInteger(this->buffer + 1, &vbiBytes);
    *lengthLength = vbiBytes; // number of VBI bytes = old (len - 1)

    if (isPublish) {
        // Read in topic length to calculate bytes to skip over for Stream writing
        if(!readByte(this->buffer, &len)) return 0;
        if(!readByte(this->buffer, &len)) return 0;
        skip = (this->buffer[*lengthLength+1]<<8)+this->buffer[*lengthLength+2];
        start = 2;
        if (this->buffer[0]&0x06) {
            // skip message id for QoS > 0
            skip += 2;
        }
    }
    uint32_t idx = len;

    if (!this->stream) {
        // ---- Fast path: bulk read directly into buffer (~10x faster) ----
        uint32_t remaining = length - start;
        uint16_t bufAvail  = (this->bufferSize > len) ? (uint16_t)(this->bufferSize - len) : 0;
        uint16_t toRead    = (remaining < (uint32_t)bufAvail) ? (uint16_t)remaining : bufAvail;

        if (toRead > 0 && !readBytes(this->buffer + len, toRead)) return 0;
        len += toRead;
        idx += remaining;

        // Drain any data that overflows the buffer (packet too large)
        if (remaining > (uint32_t)bufAvail) {
            uint8_t drain;
            for (uint32_t d = 0; d < (remaining - bufAvail); d++) {
                if (!readByte(&drain)) return 0;
            }
            len = 0; // signal oversized packet
        }
    } else {
        // ---- Slow path: byte-by-byte for Stream output ----
        uint8_t digit2;
        for (uint32_t i = start; i < length; i++) {
            if(!readByte(&digit2)) return 0;
            if (isPublish && idx - *lengthLength - 2 > skip) {
                this->stream->write(digit2);
            }
            if (len < this->bufferSize) {
                this->buffer[len] = digit2;
                len++;
            }
            idx++;
        }
        // For the stream path we do NOT discard the packet when idx exceeds bufferSize:
        // the header and as much payload as fits are already in the buffer, and the
        // overflow bytes were forwarded to the Stream. loop() will deliver both.
    }
    return len;
}

boolean PubSubClient::loop() {
    if (connected()) {
        unsigned long t = millis();
        if ((t - lastInActivity > this->keepAlive*1000UL) || (t - lastOutActivity > this->keepAlive*1000UL)) {
            if (pingOutstanding) {
                this->_state = MQTT_CONNECTION_TIMEOUT;
                _client->stop();
                return false;
            } else {
                this->buffer[0] = MQTTPINGREQ;
                this->buffer[1] = 0;
                _client->write(this->buffer,2);
                lastOutActivity = t;
                lastInActivity = t;
                pingOutstanding = true;
            }
        }
        if (_client->available()) {
            uint8_t llen;
            uint16_t len = readPacket(&llen);
            uint16_t msgId = 0;
            uint8_t *payload;
            if (len > 0) {
                lastInActivity = t;
                uint8_t type = this->buffer[0]&0xF0;
                if (type == MQTTPUBLISH) {
                    if (callback) {
                        uint16_t tl = (this->buffer[llen+1]<<8)+this->buffer[llen+2];
                        memmove(this->buffer+llen+2, this->buffer+llen+3, tl);
                        this->buffer[llen+2+tl] = 0;
                        char *topic = (char*)this->buffer+llen+2;
                        uint8_t qos = (this->buffer[0] & 0x06) >> 1;

                        if (qos == 1 || qos == 2) {
                            // QoS 1 or 2: msgId immediately after topic null-terminator
                            msgId = ((uint16_t)this->buffer[llen+3+tl] << 8) + this->buffer[llen+4+tl];
                            uint16_t afterHdr = (uint16_t)(llen + 5 + tl);
#if MQTT_ENABLE_V5
                            // Step 6: skip publish properties section for MQTT 5
                            if (_mqttVersion == MQTT_VERSION_5 && afterHdr < len) {
                                afterHdr = skipProperties(this->buffer, afterHdr);
                            }
#endif
                            payload = this->buffer + afterHdr;
                            uint16_t plength = (afterHdr < len) ? (uint16_t)(len - afterHdr) : 0;

                            if (qos == 1) {
                                callback(topic, payload, plength);
                                sendSimplePacket(MQTTPUBACK, msgId);
                                lastOutActivity = t;
                            } else {
                                int8_t slot = findPendingSlot(msgId);
                                if (slot < 0) {
                                    callback(topic, payload, plength);
                                    slot = findFreeSlot();
                                    if (slot >= 0) {
                                        pendingMessages[slot].msgId = msgId;
                                        pendingMessages[slot].state = MQTT_QOS_STATE_WAIT_PUBREL;
                                    }
                                }
                                sendSimplePacket(MQTTPUBREC, msgId);
                                lastOutActivity = t;
                            }
                        } else {
                            // QoS 0: payload starts right after topic
                            uint16_t afterHdr = (uint16_t)(llen + 3 + tl);
#if MQTT_ENABLE_V5
                            // Step 6: skip publish properties section for MQTT 5
                            if (_mqttVersion == MQTT_VERSION_5 && afterHdr < len) {
                                afterHdr = skipProperties(this->buffer, afterHdr);
                            }
#endif
                            payload = this->buffer + afterHdr;
                            uint16_t plength = (afterHdr < len) ? (uint16_t)(len - afterHdr) : 0;
                            callback(topic, payload, plength);
                        }
                    }
                } else if (type == MQTTPUBACK) {
                    // QoS 1 publish acknowledged
                    msgId = (this->buffer[llen+1]<<8)+this->buffer[llen+2];
                    int8_t slot = findPendingSlot(msgId);
                    if (slot >= 0) {
                        // Step 7: MQTT 5 — reason code present if remlen > 2 (len > llen+3)
                        // Free the slot regardless (we notified above; future: user callback on error)
                        pendingMessages[slot].state = MQTT_QOS_STATE_FREE;
                        pendingMessages[slot].msgId = 0;
                    }
                } else if (type == MQTTPUBREC) {
                    // QoS 2 outgoing: received PUBREC, send PUBREL
                    msgId = (this->buffer[llen+1]<<8)+this->buffer[llen+2];
                    int8_t slot = findPendingSlot(msgId);
#if MQTT_ENABLE_V5
                    // Step 7: if reason code present and non-success, free the slot (abort)
                    if (_mqttVersion == MQTT_VERSION_5 && len > (uint16_t)(llen + 3)) {
                        uint8_t rc5 = this->buffer[llen + 3];
                        if (rc5 >= 0x80) {
                            if (slot >= 0) {
                                pendingMessages[slot].state = MQTT_QOS_STATE_FREE;
                                pendingMessages[slot].msgId = 0;
                            }
                        } else if (slot >= 0) {
                            pendingMessages[slot].state = MQTT_QOS_STATE_WAIT_PUBCOMP;
                            sendSimplePacket(MQTTPUBREL | MQTTQOS1, msgId);
                            lastOutActivity = t;
                        }
                    } else
#endif
                    {
                        if (slot >= 0) pendingMessages[slot].state = MQTT_QOS_STATE_WAIT_PUBCOMP;
                        sendSimplePacket(MQTTPUBREL | MQTTQOS1, msgId);
                        lastOutActivity = t;
                    }
                } else if (type == MQTTPUBREL) {
                    // QoS 2 incoming: received PUBREL, send PUBCOMP
                    msgId = (this->buffer[llen+1]<<8)+this->buffer[llen+2];
                    int8_t slot = findPendingSlot(msgId);
                    if (slot >= 0) {
                        pendingMessages[slot].state = MQTT_QOS_STATE_FREE;
                        pendingMessages[slot].msgId = 0;
                    }
                    sendSimplePacket(MQTTPUBCOMP, msgId);
                    lastOutActivity = t;
                } else if (type == MQTTPUBCOMP) {
                    // QoS 2 outgoing: publish complete
                    msgId = (this->buffer[llen+1]<<8)+this->buffer[llen+2];
                    int8_t slot = findPendingSlot(msgId);
                    if (slot >= 0) {
                        pendingMessages[slot].state = MQTT_QOS_STATE_FREE;
                        pendingMessages[slot].msgId = 0;
                    }
                } else if (type == MQTTSUBACK) {
                    // Step 9: Subscribe acknowledgment
                    // msgId = buffer[llen+1..llen+2] (available if needed for future callback)
                    // MQTT 5: reason codes follow properties; MQTT 3.1.1: granted QoS values follow msgId
                    // For now: consumed silently (subscribe() is fire-and-forget)
                    (void)llen;
                } else if (type == MQTTUNSUBACK) {
                    // Step 9: Unsubscribe acknowledgment — consumed silently
                } else if (type == MQTTPINGREQ) {
                    this->buffer[0] = MQTTPINGRESP;
                    this->buffer[1] = 0;
                    _client->write(this->buffer,2);
                } else if (type == MQTTPINGRESP) {
                    pingOutstanding = false;
                }
            } else if (!connected()) {
                // readPacket has closed the connection
                return false;
            }
        }
        return true;
    }
    return false;
}

boolean PubSubClient::publish(const char* topic, const char* payload) {
    return publish(topic,(const uint8_t*)payload, payload ? strnlen(payload, this->bufferSize) : 0,false);
}

boolean PubSubClient::publish(const char* topic, const char* payload, boolean retained) {
    return publish(topic,(const uint8_t*)payload, payload ? strnlen(payload, this->bufferSize) : 0,retained);
}

boolean PubSubClient::publish(const char* topic, const uint8_t* payload, unsigned int plength) {
    return publish(topic, payload, plength, false);
}

boolean PubSubClient::publish(const char* topic, const char* payload, boolean retained, uint8_t qos) {
    return publish(topic,(const uint8_t*)payload, payload ? strnlen(payload, this->bufferSize) : 0, retained, qos);
}

boolean PubSubClient::publish(const char* topic, const uint8_t* payload, unsigned int plength, boolean retained, uint8_t qos) {
    if (!connected()) return false;
    if (qos > 2) return false;

    size_t topicLen = strnlen(topic, this->bufferSize);
    uint16_t msgIdLen = (qos > 0) ? 2 : 0;
#if MQTT_ENABLE_V5
    // Step 6: MQTT 5 PUBLISH carries a 1-byte (minimum) properties-length field
    uint16_t propsLen = (_mqttVersion == MQTT_VERSION_5) ? 1 : 0;
#else
    uint16_t propsLen = 0;
#endif
    // Check that header + topic-len-prefix + topic + msgId + props + payload all fit
    if (this->bufferSize < MQTT_MAX_HEADER_SIZE + 2 + topicLen + msgIdLen + propsLen + plength) {
        return false;
    }
    uint16_t length = MQTT_MAX_HEADER_SIZE;
    length = writeString(topic, this->buffer, length);

    uint16_t currentMsgId = 0;
    if (qos > 0) {
        currentMsgId = nextMsgId++;
        if (nextMsgId == 0) nextMsgId = 1;
        this->buffer[length++] = (currentMsgId >> 8);
        this->buffer[length++] = (currentMsgId & 0xFF);
    }

#if MQTT_ENABLE_V5
    // Step 6: insert publish properties section (empty for now)
    if (_mqttVersion == MQTT_VERSION_5) {
        this->buffer[length++] = 0x00; // Properties Length = 0
    }
#endif

    // Fast payload copy
    memcpy(this->buffer + length, payload, plength);
    length += (uint16_t)plength;

    uint8_t header = MQTTPUBLISH;
    if (retained)    header |= 1;
    if (qos == 1)    header |= MQTTQOS1;
    else if (qos==2) header |= MQTTQOS2;

    boolean result = write(header, this->buffer, length - MQTT_MAX_HEADER_SIZE);

    if (result && qos > 0) {
        int8_t slot = findFreeSlot();
        if (slot >= 0) {
            pendingMessages[slot].msgId = currentMsgId;
            pendingMessages[slot].state = (qos == 1)
                ? MQTT_QOS_STATE_WAIT_PUBACK
                : MQTT_QOS_STATE_WAIT_PUBREC;
        }
    }
    return result;
}

boolean PubSubClient::publish(const char* topic, const uint8_t* payload, unsigned int plength, boolean retained) {
    // Delegate to QoS-aware version (QoS 0 = no msgId, no tracking)
    return publish(topic, payload, plength, retained, 0);
}

boolean PubSubClient::publish_P(const char* topic, const char* payload, boolean retained) {
    return publish_P(topic, (const uint8_t*)payload, payload ? strnlen(payload, this->bufferSize) : 0, retained);
}

boolean PubSubClient::publish_P(const char* topic, const uint8_t* payload, unsigned int plength, boolean retained) {
    if (!connected()) return false;

    uint16_t tlen = (uint16_t)strnlen(topic, this->bufferSize);

    // Build fixed header + topic using existing helpers
    uint16_t pos = MQTT_MAX_HEADER_SIZE;
    pos = writeString(topic, this->buffer, pos);

    uint8_t header = MQTTPUBLISH;
    if (retained) header |= 1;

    // buildHeader writes into buffer[0..MQTT_MAX_HEADER_SIZE-1]
    size_t hlen = buildHeader(header, this->buffer, plength + 2 + tlen);
    uint16_t startPos = (uint16_t)(MQTT_MAX_HEADER_SIZE - hlen);
    uint16_t headerAndTopic = pos - startPos;

    uint16_t rc = _client->write(this->buffer + startPos, headerAndTopic);
    lastOutActivity = millis();

    // Send PROGMEM payload in 32-byte chunks to minimise per-byte call overhead
    uint8_t chunk[32];
    unsigned int sent = 0;
    while (sent < plength) {
        uint8_t chunkLen = ((plength - sent) < sizeof(chunk)) ? (uint8_t)(plength - sent) : (uint8_t)sizeof(chunk);
        for (uint8_t i = 0; i < chunkLen; i++) {
            chunk[i] = pgm_read_byte_near(payload + sent + i);
        }
        rc += _client->write(chunk, chunkLen);
        sent += chunkLen;
    }

    return (rc == (uint16_t)(headerAndTopic + plength));
}

boolean PubSubClient::beginPublish(const char* topic, unsigned int plength, boolean retained) {
    if (connected()) {
        // Send the header and variable length field
        uint16_t length = MQTT_MAX_HEADER_SIZE;
        length = writeString(topic,this->buffer,length);
        uint8_t header = MQTTPUBLISH;
        if (retained) {
            header |= 1;
        }
        size_t hlen = buildHeader(header, this->buffer, plength+length-MQTT_MAX_HEADER_SIZE);
        uint16_t rc = _client->write(this->buffer+(MQTT_MAX_HEADER_SIZE-hlen),length-(MQTT_MAX_HEADER_SIZE-hlen));
        lastOutActivity = millis();
        return (rc == (length-(MQTT_MAX_HEADER_SIZE-hlen)));
    }
    return false;
}

int PubSubClient::endPublish() {
 return 1;
}

size_t PubSubClient::write(uint8_t data) {
    lastOutActivity = millis();
    return _client->write(data);
}

size_t PubSubClient::write(const uint8_t *buffer, size_t size) {
    lastOutActivity = millis();
    return _client->write(buffer,size);
}

size_t PubSubClient::buildHeader(uint8_t header, uint8_t* buf, uint16_t length) {
    uint8_t lenBuf[4];
    uint8_t llen = encodeVariableByteInteger(length, lenBuf);
    buf[4 - llen] = header;
    for (uint8_t i = 0; i < llen; i++) {
        buf[MQTT_MAX_HEADER_SIZE - llen + i] = lenBuf[i];
    }
    return llen + 1; // Full header size is variable length bits plus the 1-byte fixed header
}

boolean PubSubClient::write(uint8_t header, uint8_t* buf, uint16_t length) {
    uint16_t rc;
    uint8_t hlen = buildHeader(header, buf, length);

#ifdef MQTT_MAX_TRANSFER_SIZE
    uint8_t* writeBuf = buf+(MQTT_MAX_HEADER_SIZE-hlen);
    uint16_t bytesRemaining = length+hlen;  //Match the length type
    uint8_t bytesToWrite;
    boolean result = true;
    while((bytesRemaining > 0) && result) {
        bytesToWrite = (bytesRemaining > MQTT_MAX_TRANSFER_SIZE)?MQTT_MAX_TRANSFER_SIZE:bytesRemaining;
        rc = _client->write(writeBuf,bytesToWrite);
        result = (rc == bytesToWrite);
        bytesRemaining -= rc;
        writeBuf += rc;
    }
    return result;
#else
    rc = _client->write(buf+(MQTT_MAX_HEADER_SIZE-hlen),length+hlen);
    lastOutActivity = millis();
    return (rc == hlen+length);
#endif
}

boolean PubSubClient::subscribe(const char* topic) {
    return subscribe(topic, 0);
}

boolean PubSubClient::subscribe(const char* topic, uint8_t qos) {
    size_t topicLength = strnlen(topic, this->bufferSize);
    if (topic == 0) {
        return false;
    }
    if (qos > 2) {
        return false;
    }
#if MQTT_ENABLE_V5
    // Step 8: MQTT 5 SUBSCRIBE requires 1 extra byte for the properties-length field
    uint16_t minBuf = (_mqttVersion == MQTT_VERSION_5) ? (uint16_t)(10 + topicLength)
                                                        : (uint16_t)(9 + topicLength);
#else
    uint16_t minBuf = (uint16_t)(9 + topicLength);
#endif
    if (this->bufferSize < minBuf) {
        return false;
    }
    if (connected()) {
        uint16_t length = MQTT_MAX_HEADER_SIZE;
        nextMsgId++;
        if (nextMsgId == 0) {
            nextMsgId = 1;
        }
        this->buffer[length++] = (nextMsgId >> 8);
        this->buffer[length++] = (nextMsgId & 0xFF);
#if MQTT_ENABLE_V5
        // Step 8: properties section (empty)
        if (_mqttVersion == MQTT_VERSION_5) {
            this->buffer[length++] = 0x00; // Properties Length = 0
        }
#endif
        length = writeString((char*)topic, this->buffer, length);
        this->buffer[length++] = qos; // bits 0-1: QoS; bits 2-5 (MQTT5 options): all 0
        return write(MQTTSUBSCRIBE|MQTTQOS1, this->buffer, length-MQTT_MAX_HEADER_SIZE);
    }
    return false;
}

boolean PubSubClient::unsubscribe(const char* topic) {
    size_t topicLength = strnlen(topic, this->bufferSize);
    if (topic == 0) {
        return false;
    }
#if MQTT_ENABLE_V5
    // Step 8: MQTT 5 UNSUBSCRIBE requires 1 extra byte for properties-length field
    uint16_t minBuf = (_mqttVersion == MQTT_VERSION_5) ? (uint16_t)(10 + topicLength)
                                                        : (uint16_t)(9 + topicLength);
#else
    uint16_t minBuf = (uint16_t)(9 + topicLength);
#endif
    if (this->bufferSize < minBuf) {
        return false;
    }
    if (connected()) {
        uint16_t length = MQTT_MAX_HEADER_SIZE;
        nextMsgId++;
        if (nextMsgId == 0) {
            nextMsgId = 1;
        }
        this->buffer[length++] = (nextMsgId >> 8);
        this->buffer[length++] = (nextMsgId & 0xFF);
#if MQTT_ENABLE_V5
        // Step 8: properties section (empty)
        if (_mqttVersion == MQTT_VERSION_5) {
            this->buffer[length++] = 0x00; // Properties Length = 0
        }
#endif
        length = writeString(topic, this->buffer, length);
        return write(MQTTUNSUBSCRIBE|MQTTQOS1, this->buffer, length-MQTT_MAX_HEADER_SIZE);
    }
    return false;
}

void PubSubClient::disconnect() {
    this->buffer[0] = MQTTDISCONNECT;
    this->buffer[1] = 0;
    _client->write(this->buffer,2);
    _state = MQTT_DISCONNECTED;
    _client->flush();
    _client->stop();
    lastInActivity = lastOutActivity = millis();
    clearPendingMessages();
}

uint16_t PubSubClient::writeString(const char* string, uint8_t* buf, uint16_t pos) {
    // Use strlen + memcpy: much faster than char-by-char loop
    uint16_t len = (uint16_t)strlen(string);
    buf[pos++] = (len >> 8);
    buf[pos++] = (len & 0xFF);
    memcpy(buf + pos, string, len);
    return pos + len;
}


boolean PubSubClient::connected() {
    boolean rc;
    if (_client == NULL ) {
        rc = false;
    } else {
        rc = (int)_client->connected();
        if (!rc) {
            if (this->_state == MQTT_CONNECTED) {
                this->_state = MQTT_CONNECTION_LOST;
                _client->flush();
                _client->stop();
            }
        } else {
            return this->_state == MQTT_CONNECTED;
        }
    }
    return rc;
}

PubSubClient& PubSubClient::setServer(uint8_t * ip, uint16_t port) {
    IPAddress addr(ip[0],ip[1],ip[2],ip[3]);
    return setServer(addr,port);
}

PubSubClient& PubSubClient::setServer(IPAddress ip, uint16_t port) {
    this->ip = ip;
    this->port = port;
    this->domain = NULL;
    return *this;
}

PubSubClient& PubSubClient::setServer(const char * domain, uint16_t port) {
    this->domain = domain;
    this->port = port;
    return *this;
}

PubSubClient& PubSubClient::setCallback(MQTT_CALLBACK_SIGNATURE) {
    this->callback = callback;
    return *this;
}

PubSubClient& PubSubClient::setClient(Client& client){
    this->_client = &client;
    return *this;
}

PubSubClient& PubSubClient::setStream(Stream& stream){
    this->stream = &stream;
    return *this;
}

int PubSubClient::state() {
    return this->_state;
}

boolean PubSubClient::setBufferSize(uint16_t size) {
    if (size == 0) return false;

    if (this->bufferSize == 0) {
        this->buffer = (uint8_t*)malloc(size);
        if (this->buffer == NULL) return false;
        this->bufferSize = size;
    } else {
        uint8_t* newBuffer = (uint8_t*)realloc(this->buffer, size);
        if (newBuffer == NULL) return false; // keep old buffer intact
        this->buffer = newBuffer;
        this->bufferSize = size;
    }
    return true;
}

uint16_t PubSubClient::getBufferSize() {
    return this->bufferSize;
}
PubSubClient& PubSubClient::setKeepAlive(uint16_t keepAlive) {
    this->keepAlive = keepAlive;
    return *this;
}
PubSubClient& PubSubClient::setSocketTimeout(uint16_t timeout) {
    this->socketTimeout = timeout;
    return *this;
}

boolean PubSubClient::sendSimplePacket(uint8_t type, uint16_t msgId) {
    if (this->bufferSize < 4) return false; // safety guard
    this->buffer[0] = type;
    this->buffer[1] = 2;
    this->buffer[2] = (msgId >> 8);
    this->buffer[3] = (msgId & 0xFF);
    uint16_t rc = _client->write(this->buffer, 4);
    lastOutActivity = millis();
    return (rc == 4);
}

int8_t PubSubClient::findPendingSlot(uint16_t msgId) {
    for (uint8_t i = 0; i < MQTT_MAX_QOS_PENDING; i++) {
        if (pendingMessages[i].msgId == msgId && pendingMessages[i].state != MQTT_QOS_STATE_FREE) {
            return (int8_t)i;
        }
    }
    return -1;
}

int8_t PubSubClient::findFreeSlot() {
    for (uint8_t i = 0; i < MQTT_MAX_QOS_PENDING; i++) {
        if (pendingMessages[i].state == MQTT_QOS_STATE_FREE) {
            return (int8_t)i;
        }
    }
    return -1;
}

void PubSubClient::clearPendingMessages() {
    for (uint8_t i = 0; i < MQTT_MAX_QOS_PENDING; i++) {
        pendingMessages[i].state = MQTT_QOS_STATE_FREE;
        pendingMessages[i].msgId = 0;
    }
}

// -----------------------------------------------------------------------
// Step 2: Variable Byte Integer encode / decode
// -----------------------------------------------------------------------

// Encode 'value' as a MQTT Variable Byte Integer into buf[0..3].
// Returns the number of bytes written (1-4).
uint8_t PubSubClient::encodeVariableByteInteger(uint32_t value, uint8_t* buf) {
    uint8_t len = 0;
    do {
        uint8_t digit = value & 0x7F;
        value >>= 7;
        if (value > 0) digit |= 0x80;
        buf[len++] = digit;
    } while (value > 0);
    return len;
}

// Decode a Variable Byte Integer from buf.
// *bytesUsed is set to the number of bytes consumed (1-4).
uint32_t PubSubClient::decodeVariableByteInteger(const uint8_t* buf, uint8_t* bytesUsed) {
    uint32_t value = 0;
    uint32_t multiplier = 1;
    uint8_t pos = 0;
    uint8_t digit;
    do {
        digit = buf[pos++];
        value += (uint32_t)(digit & 0x7F) * multiplier;
        multiplier <<= 7;
    } while ((digit & 0x80) != 0 && pos < 4);
    *bytesUsed = pos;
    return value;
}

#if MQTT_ENABLE_V5

// -----------------------------------------------------------------------
// Step 1: setMqttVersion / getMqttVersion
// -----------------------------------------------------------------------
PubSubClient& PubSubClient::setMqttVersion(uint8_t version) {
    this->_mqttVersion = version;
    return *this;
}

uint8_t PubSubClient::getMqttVersion() const {
    return this->_mqttVersion;
}

// -----------------------------------------------------------------------
// Step 3: Property write helpers
// All helpers write directly into buf[] at pos and return the new pos.
// They silently skip writing if there is insufficient space.
// -----------------------------------------------------------------------

uint16_t PubSubClient::writePropertyU8(uint8_t id, uint8_t value, uint8_t* buf, uint16_t pos) {
    if (pos + 2 > this->bufferSize) return pos;
    buf[pos++] = id;
    buf[pos++] = value;
    return pos;
}

uint16_t PubSubClient::writePropertyU16(uint8_t id, uint16_t value, uint8_t* buf, uint16_t pos) {
    if (pos + 3 > this->bufferSize) return pos;
    buf[pos++] = id;
    buf[pos++] = (value >> 8);
    buf[pos++] = (value & 0xFF);
    return pos;
}

uint16_t PubSubClient::writePropertyU32(uint8_t id, uint32_t value, uint8_t* buf, uint16_t pos) {
    if (pos + 5 > this->bufferSize) return pos;
    buf[pos++] = id;
    buf[pos++] = (uint8_t)(value >> 24);
    buf[pos++] = (uint8_t)(value >> 16);
    buf[pos++] = (uint8_t)(value >> 8);
    buf[pos++] = (uint8_t)(value & 0xFF);
    return pos;
}

uint16_t PubSubClient::writePropertyStr(uint8_t id, const char* str, uint8_t* buf, uint16_t pos) {
    uint16_t slen = (uint16_t)strlen(str);
    if (pos + 3u + slen > this->bufferSize) return pos;
    buf[pos++] = id;
    buf[pos++] = (slen >> 8);
    buf[pos++] = (slen & 0xFF);
    memcpy(buf + pos, str, slen);
    return pos + slen;
}

uint16_t PubSubClient::writePropertyBin(uint8_t id, const uint8_t* data, uint16_t len,
                                         uint8_t* buf, uint16_t pos) {
    if (pos + 3u + len > this->bufferSize) return pos;
    buf[pos++] = id;
    buf[pos++] = (len >> 8);
    buf[pos++] = (len & 0xFF);
    memcpy(buf + pos, data, len);
    return pos + len;
}

uint16_t PubSubClient::writePropertyVBI(uint8_t id, uint32_t value, uint8_t* buf, uint16_t pos) {
    uint8_t vbi[4];
    uint8_t vlen = encodeVariableByteInteger(value, vbi);
    if (pos + 1u + vlen > this->bufferSize) return pos;
    buf[pos++] = id;
    memcpy(buf + pos, vbi, vlen);
    return pos + vlen;
}

// -----------------------------------------------------------------------
// Step 3: Property read helpers
// -----------------------------------------------------------------------

// Advance pos past a single property value.
// Returns the position immediately after the value; returns pos unchanged
// for unknown property IDs (caller should treat as end-of-properties).
uint16_t PubSubClient::skipPropertyValue(uint8_t propId, const uint8_t* buf, uint16_t pos) {
    switch (propId) {
        // --- 1-byte (Byte) properties ---
        case 0x01: case 0x17: case 0x19: case 0x24:
        case 0x25: case 0x28: case 0x29: case 0x2A:
            return pos + 1;
        // --- 2-byte (Two Byte Integer) properties ---
        case 0x13: case 0x21: case 0x22: case 0x23:
            return pos + 2;
        // --- 4-byte (Four Byte Integer) properties ---
        case 0x02: case 0x11: case 0x18: case 0x27:
            return pos + 4;
        // --- Variable Byte Integer ---
        case 0x0B: {
            uint8_t used;
            decodeVariableByteInteger(buf + pos, &used);
            return pos + used;
        }
        // --- UTF-8 String and Binary Data (2-byte length prefix) ---
        case 0x03: case 0x08: case 0x09: case 0x12:
        case 0x15: case 0x16: case 0x1A: case 0x1C: case 0x1F: {
            uint16_t slen = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
            return pos + 2 + slen;
        }
        // --- UTF-8 String Pair (User Property) ---
        case 0x26: {
            uint16_t len1 = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
            uint16_t after = pos + 2 + len1;
            uint16_t len2 = ((uint16_t)buf[after] << 8) | buf[after + 1];
            return after + 2 + len2;
        }
        default:
            // Unknown property ID: cannot safely advance. Return pos unchanged
            // so the caller can detect the stall and stop scanning.
            return pos;
    }
}

// Advance pos past an entire properties section:
//   pos points to the VBI property-length field.
// Returns position immediately after the whole section.
uint16_t PubSubClient::skipProperties(const uint8_t* buf, uint16_t pos) {
    uint8_t vbiBytes;
    uint32_t propLen = decodeVariableByteInteger(buf + pos, &vbiBytes);
    return pos + vbiBytes + (uint16_t)propLen;
}

// Scan a properties section for a specific property ID.
//   buf            : packet buffer
//   propsPayloadStart : position of the first property identifier byte (after the VBI length)
//   propsPayloadEnd   : position just past the last byte of the properties section
//   id             : the property identifier to search for
//   valueOut       : if non-NULL, set to the start of the property value
//   valueLenOut    : if non-NULL, set to the encoded byte length of the value
// Returns true if found.
bool PubSubClient::findProperty(const uint8_t* buf,
                                 uint16_t propsPayloadStart, uint16_t propsPayloadEnd,
                                 uint8_t id,
                                 const uint8_t** valueOut, uint16_t* valueLenOut) {
    uint16_t pos = propsPayloadStart;
    while (pos < propsPayloadEnd) {
        uint8_t propId = buf[pos++];
        uint16_t valueStart = pos;
        uint16_t after = skipPropertyValue(propId, buf, pos);
        if (after == pos) break; // unknown id — cannot advance safely, stop
        if (propId == id) {
            if (valueOut)    *valueOut    = buf + valueStart;
            if (valueLenOut) *valueLenOut = after - valueStart;
            return true;
        }
        pos = after;
    }
    return false;
}

#endif // MQTT_ENABLE_V5
