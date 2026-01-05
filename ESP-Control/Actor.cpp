#include "Actor.h"
#include "ESPNowComm.h"
#include "Security.h"
#include "Discovery.h"
#include <string.h>

Actor g_actor;

Actor::Actor() : m_actorId(0), m_relayPin(0), m_relayState(false), m_lastBeaconTime(0) {
  memset(&m_enablePin, 0, sizeof(m_enablePin));
  memset(m_indicators, 0, sizeof(m_indicators));
}

void Actor::begin(uint8_t actorId, uint8_t relayPin,
                  const EnablePinConfig& enablePin,
                  const IndicatorConfig indicators[3]) {
  m_actorId = actorId;
  m_relayPin = relayPin;

  // Relais-Pin konfigurieren
  pinMode(m_relayPin, OUTPUT);
  digitalWrite(m_relayPin, LOW);  // Default: AUS
  m_relayState = false;

  // Enable-Pin konfigurieren
  m_enablePin = enablePin;
  if (m_enablePin.enabled) {
    pinMode(m_enablePin.pin, m_enablePin.activeLow ? INPUT_PULLUP : INPUT);
  }

  // Indikatoren konfigurieren
  for (int i = 0; i < 3; i++) {
    m_indicators[i] = indicators[i];
    if (m_indicators[i].enabled && m_indicators[i].type == INDICATOR_LED) {
      pinMode(m_indicators[i].pin, OUTPUT);
      setIndicator(i, false);
    }
    // TODO: Pixel-Unterstützung (NeoPixel, etc.)
  }

  DEBUG_PRINTF("[Actor] Initialisiert (ID=%d, Relais-Pin=%d)\n", m_actorId, m_relayPin);
}

void Actor::update() {
  // Beacon senden (nur wenn enabled)
  if (isEnabled()) {
    g_discovery.actorSendBeacon(m_actorId, m_relayState);
  }

  // Indikatoren aktualisieren
  updateIndicators();
}

void Actor::setRelay(bool state) {
  m_relayState = state;
  digitalWrite(m_relayPin, state ? HIGH : LOW);

  DEBUG_PRINTF("[Actor] Relais: %s\n", state ? "EIN" : "AUS");
}

void Actor::toggleRelay() {
  setRelay(!m_relayState);
}

bool Actor::isEnabled() const {
  if (!m_enablePin.enabled) {
    return true;  // immer sichtbar, wenn kein Enable-Pin
  }

  bool pinState = digitalRead(m_enablePin.pin);
  return (m_enablePin.activeLow ? !pinState : pinState);
}

void Actor::updateIndicators() {
  // Indikator 1: Relais-Status
  if (m_indicators[0].enabled) {
    setIndicator(0, m_relayState);
  }

  // Indikator 2: Enabled-Status
  if (m_indicators[1].enabled) {
    setIndicator(1, isEnabled());
  }

  // Indikator 3: ESP-NOW aktiv
  if (m_indicators[2].enabled) {
    setIndicator(2, g_espNow.isInitialized());
  }
}

void Actor::setIndicator(int index, bool state) {
  if (index < 0 || index >= 3 || !m_indicators[index].enabled) {
    return;
  }

  if (m_indicators[index].type == INDICATOR_LED) {
    bool outputState = m_indicators[index].activeLow ? !state : state;
    digitalWrite(m_indicators[index].pin, outputState ? HIGH : LOW);
  }
  // TODO: Pixel-Unterstützung
}

void Actor::handleMessage(const uint8_t* mac, const uint8_t* data, int len) {
  if (len < sizeof(MessageHeader)) {
    return;
  }

  const MessageHeader* header = (const MessageHeader*)data;

  // Prüfen ob Nachricht für uns bestimmt ist
  if (header->actorId != m_actorId && header->actorId != 0xFF) {
    return;
  }

  // Nachricht verarbeiten
  switch (header->msgType) {
    case MSG_RELAY_TOGGLE:
      if (len == sizeof(RelayToggleMessage)) {
        handleRelayToggle(mac, (const RelayToggleMessage*)data);
      }
      break;

    case MSG_OTA_PREPARE:
      if (len == sizeof(OTAPrepareMessage)) {
        handleOTAPrepare(mac, (const OTAPrepareMessage*)data);
      }
      break;

    case MSG_CHALLENGE:
      if (len == sizeof(ChallengeMessage)) {
        handleChallenge(mac, (const ChallengeMessage*)data);
      }
      break;

    default:
      break;
  }
}

void Actor::handleRelayToggle(const uint8_t* mac, const RelayToggleMessage* msg) {
  // Sicherheit: Nonce prüfen
  if (!g_security.validateNonce(msg->header.nonce, msg->header.controllerId)) {
    DEBUG_PRINTLN("[Actor] Replay erkannt (RelayToggle)");
    return;
  }

  // Sicherheit: HMAC prüfen
  if (!g_security.validateCommandHMAC(&msg->header, msg->hmac)) {
    DEBUG_PRINTLN("[Actor] HMAC ungültig (RelayToggle)");
    return;
  }

  // Relais umschalten
  toggleRelay();

  // Status zurücksenden
  RelayStateMessage response = {};
  response.header.msgType = MSG_RELAY_STATE;
  response.header.actorId = m_actorId;
  response.header.controllerId = msg->header.controllerId;
  response.header.timestamp = millis();
  response.header.nonce = g_security.generateNonce();
  response.relayState = m_relayState;

  g_espNow.sendUnicast(mac, (uint8_t*)&response, sizeof(response));

  DEBUG_PRINTF("[Actor] Relais-Toggle von Controller %d\n", msg->header.controllerId);
}

void Actor::handleOTAPrepare(const uint8_t* mac, const OTAPrepareMessage* msg) {
  // Sicherheit: Nonce prüfen
  if (!g_security.validateNonce(msg->header.nonce, msg->header.controllerId)) {
    DEBUG_PRINTLN("[Actor] Replay erkannt (OTAPrepare)");
    return;
  }

  // Sicherheit: HMAC prüfen
  if (!g_security.validateCommandHMAC(&msg->header, msg->hmac)) {
    DEBUG_PRINTLN("[Actor] HMAC ungültig (OTAPrepare)");
    return;
  }

  // ACK senden
  OTAAckMessage ack = {};
  ack.header.msgType = MSG_OTA_ACK;
  ack.header.actorId = m_actorId;
  ack.header.controllerId = msg->header.controllerId;
  ack.header.timestamp = millis();
  ack.header.nonce = g_security.generateNonce();

  g_espNow.sendUnicast(mac, (uint8_t*)&ack, sizeof(ack));

  DEBUG_PRINTLN("[Actor] OTA-Prepare empfangen, ACK gesendet");

  // Systemzustand auf OTA setzen (wird in main loop behandelt)
  extern SystemState g_systemState;
  g_systemState = STATE_OTA_PREPARE;
}

void Actor::handleChallenge(const uint8_t* mac, const ChallengeMessage* msg) {
  // Challenge beantworten
  ResponseMessage response = {};
  response.header.msgType = MSG_RESPONSE;
  response.header.actorId = m_actorId;
  response.header.controllerId = msg->header.controllerId;
  response.header.timestamp = millis();
  response.header.nonce = g_security.generateNonce();

  if (!g_security.generateResponse(msg->challenge, response.response)) {
    DEBUG_PRINTLN("[Actor] Fehler beim Generieren der Response");
    return;
  }

  g_espNow.sendUnicast(mac, (uint8_t*)&response, sizeof(response));

  DEBUG_PRINTF("[Actor] Challenge beantwortet (Controller %d)\n", msg->header.controllerId);
}
