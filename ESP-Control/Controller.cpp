#include "Controller.h"
#include "ESPNowComm.h"
#include "Security.h"
#include "Discovery.h"
#include <string.h>

Controller g_controller;

Controller::Controller() : m_controllerId(0), m_actorCount(0) {
  memset(m_actorIds, 0, sizeof(m_actorIds));
  memset(m_buttons, 0, sizeof(m_buttons));
  memset(m_leds, 0, sizeof(m_leds));
  memset(&m_display, 0, sizeof(m_display));
  memset(m_buttonStates, 0, sizeof(m_buttonStates));
}

void Controller::begin(uint8_t controllerId,
                       const uint8_t actorIds[], int actorCount,
                       const ButtonConfig buttons[3],
                       const IndicatorConfig leds[3],
                       const DisplayConfig& display) {
  m_controllerId = controllerId;

  // Actor-IDs speichern
  m_actorCount = (actorCount > 10) ? 10 : actorCount;
  memcpy(m_actorIds, actorIds, m_actorCount);

  // Taster konfigurieren
  for (int i = 0; i < 3; i++) {
    m_buttons[i] = buttons[i];
    if (m_buttons[i].enabled) {
      pinMode(m_buttons[i].pin, m_buttons[i].activeLow ? INPUT_PULLUP : INPUT);
      m_buttonStates[i].lastState = digitalRead(m_buttons[i].pin);
      m_buttonStates[i].lastChangeTime = millis();
      m_buttonStates[i].pressStartTime = 0;
      m_buttonStates[i].longPressTriggered = false;
    }
  }

  // LEDs konfigurieren
  for (int i = 0; i < 3; i++) {
    m_leds[i] = leds[i];
    if (m_leds[i].enabled && m_leds[i].type == INDICATOR_LED) {
      pinMode(m_leds[i].pin, OUTPUT);
      setLED(i, false);
    }
  }

  // Display konfigurieren
  m_display = display;
  // TODO: Display-Initialisierung (SSD1306, etc.)

  DEBUG_PRINTF("[Controller] Initialisiert (ID=%d, %d Actoren)\n", m_controllerId, m_actorCount);
}

void Controller::update() {
  // Discovery aktualisieren
  g_discovery.controllerUpdate();

  // Taster auswerten
  updateButtons();

  // Indikatoren aktualisieren
  updateIndicators();
}

void Controller::updateButtons() {
  uint32_t now = millis();

  for (int i = 0; i < 3; i++) {
    if (!m_buttons[i].enabled) {
      continue;
    }

    bool currentState = digitalRead(m_buttons[i].pin);
    bool isPressed = m_buttons[i].activeLow ? !currentState : currentState;

    // Entprellung
    if (currentState != m_buttonStates[i].lastState) {
      m_buttonStates[i].lastChangeTime = now;
      m_buttonStates[i].lastState = currentState;
    }

    if (now - m_buttonStates[i].lastChangeTime < DEBOUNCE_MS) {
      continue;
    }

    // Taster gedrückt
    if (isPressed) {
      if (m_buttonStates[i].pressStartTime == 0) {
        m_buttonStates[i].pressStartTime = now;
        m_buttonStates[i].longPressTriggered = false;
      }

      // Long Press prüfen
      if (!m_buttonStates[i].longPressTriggered &&
          now - m_buttonStates[i].pressStartTime >= LONG_PRESS_MS) {
        m_buttonStates[i].longPressTriggered = true;
        onButtonLongPress(i);
      }
    }
    // Taster losgelassen
    else {
      if (m_buttonStates[i].pressStartTime > 0 &&
          !m_buttonStates[i].longPressTriggered) {
        // Short Press
        onButtonShortPress(i);
      }
      m_buttonStates[i].pressStartTime = 0;
      m_buttonStates[i].longPressTriggered = false;
    }
  }
}

void Controller::updateIndicators() {
  // LED 1: Anzahl entdeckter Actoren
  if (m_leds[0].enabled) {
    setLED(0, g_discovery.getDiscoveredActorCount() > 0);
  }

  // LED 2: ESP-NOW aktiv
  if (m_leds[1].enabled) {
    setLED(1, g_espNow.isInitialized());
  }

  // LED 3: Relais-Status des ersten Actors
  if (m_leds[2].enabled && m_actorCount > 0) {
    const DiscoveredActor* actor = g_discovery.findActorById(m_actorIds[0]);
    setLED(2, actor && actor->relayState);
  }

  // TODO: Display aktualisieren
}

void Controller::setLED(int index, bool state) {
  if (index < 0 || index >= 3 || !m_leds[index].enabled) {
    return;
  }

  if (m_leds[index].type == INDICATOR_LED) {
    bool outputState = m_leds[index].activeLow ? !state : state;
    digitalWrite(m_leds[index].pin, outputState ? HIGH : LOW);
  }
}

void Controller::onButtonShortPress(int buttonIndex) {
  DEBUG_PRINTF("[Controller] Short Press auf Taster %d\n", buttonIndex);

  // Zuordnung: Taster -> Actor
  if (buttonIndex < m_actorCount) {
    uint8_t actorId = m_actorIds[buttonIndex];

    // Prüfen ob Actor in Reichweite
    const DiscoveredActor* actor = g_discovery.findActorById(actorId);
    if (actor) {
      sendRelayToggle(actorId);
    } else {
      DEBUG_PRINTF("[Controller] Actor %d nicht in Reichweite\n", actorId);
    }
  }
}

void Controller::onButtonLongPress(int buttonIndex) {
  DEBUG_PRINTF("[Controller] Long Press auf Taster %d - OTA-Modus\n", buttonIndex);
  startOTAMode();
}

void Controller::sendRelayToggle(uint8_t actorId) {
  const DiscoveredActor* actor = g_discovery.findActorById(actorId);
  if (!actor) {
    return;
  }

  // Relais-Toggle-Nachricht erstellen
  RelayToggleMessage msg = {};
  msg.header.msgType = MSG_RELAY_TOGGLE;
  msg.header.controllerId = m_controllerId;
  msg.header.actorId = actorId;
  msg.header.timestamp = millis();
  msg.header.nonce = g_security.generateNonce();

  // HMAC generieren
  if (!g_security.generateCommandHMAC(&msg.header, msg.hmac)) {
    DEBUG_PRINTLN("[Controller] Fehler beim Generieren des HMAC");
    return;
  }

  // Senden
  g_espNow.sendUnicast(actor->mac, (uint8_t*)&msg, sizeof(msg));

  DEBUG_PRINTF("[Controller] Relay-Toggle an Actor %d gesendet\n", actorId);
}

void Controller::startOTAMode() {
  DEBUG_PRINTLN("[Controller] Starte OTA-Modus...");

  // OTA-Prepare an alle erreichbaren Actoren senden
  int actorCount = g_discovery.getDiscoveredActorCount();

  for (int i = 0; i < actorCount; i++) {
    const DiscoveredActor* actor = g_discovery.getDiscoveredActor(i);
    if (!actor) {
      continue;
    }

    // OTA-Prepare-Nachricht erstellen
    OTAPrepareMessage msg = {};
    msg.header.msgType = MSG_OTA_PREPARE;
    msg.header.controllerId = m_controllerId;
    msg.header.actorId = actor->actorId;
    msg.header.timestamp = millis();
    msg.header.nonce = g_security.generateNonce();

    // HMAC generieren
    if (!g_security.generateCommandHMAC(&msg.header, msg.hmac)) {
      DEBUG_PRINTLN("[Controller] Fehler beim Generieren des HMAC");
      continue;
    }

    // Senden
    g_espNow.sendUnicast(actor->mac, (uint8_t*)&msg, sizeof(msg));

    DEBUG_PRINTF("[Controller] OTA-Prepare an Actor %d gesendet\n", actor->actorId);
  }

  // Systemzustand auf OTA setzen (wird in main loop behandelt)
  extern SystemState g_systemState;
  g_systemState = STATE_OTA_PREPARE;
}

void Controller::handleMessage(const uint8_t* mac, const uint8_t* data, int len) {
  if (len < sizeof(MessageHeader)) {
    return;
  }

  const MessageHeader* header = (const MessageHeader*)data;

  // Nachricht verarbeiten
  switch (header->msgType) {
    case MSG_BEACON:
      if (len == sizeof(BeaconMessage)) {
        handleBeacon(mac, (const BeaconMessage*)data);
      }
      break;

    case MSG_RELAY_STATE:
      if (len == sizeof(RelayStateMessage)) {
        handleRelayState(mac, (const RelayStateMessage*)data);
      }
      break;

    case MSG_RESPONSE:
      if (len == sizeof(ResponseMessage)) {
        handleResponse(mac, (const ResponseMessage*)data);
      }
      break;

    case MSG_OTA_ACK:
      if (len == sizeof(OTAAckMessage)) {
        handleOTAAck(mac, (const OTAAckMessage*)data);
      }
      break;

    default:
      break;
  }
}

void Controller::handleBeacon(const uint8_t* mac, const BeaconMessage* msg) {
  // Prüfen ob dieser Actor für uns relevant ist
  bool isRelevant = false;
  for (int i = 0; i < m_actorCount; i++) {
    if (m_actorIds[i] == msg->actorId) {
      isRelevant = true;
      break;
    }
  }

  if (!isRelevant) {
    return;
  }

  // Actor registrieren/aktualisieren
  g_discovery.registerActor(msg->actorId, mac, msg->relayState, msg->rssi);

  DEBUG_PRINTF("[Controller] Beacon von Actor %d empfangen (RSSI=%d)\n",
               msg->actorId, msg->rssi);

  // TODO: Authentifizierung starten (Challenge senden)
  // authenticateActor(msg->actorId);
}

void Controller::handleRelayState(const uint8_t* mac, const RelayStateMessage* msg) {
  DEBUG_PRINTF("[Controller] Relay-State von Actor %d: %s\n",
               msg->header.actorId, msg->relayState ? "EIN" : "AUS");

  // Discovery aktualisieren
  const DiscoveredActor* actor = g_discovery.findActorById(msg->header.actorId);
  if (actor) {
    g_discovery.registerActor(msg->header.actorId, mac, msg->relayState, actor->rssi);
  }
}

void Controller::handleResponse(const uint8_t* mac, const ResponseMessage* msg) {
  DEBUG_PRINTF("[Controller] Response von Actor %d empfangen\n", msg->header.actorId);

  // TODO: Response validieren und Actor als authentifiziert markieren
}

void Controller::handleOTAAck(const uint8_t* mac, const OTAAckMessage* msg) {
  DEBUG_PRINTF("[Controller] OTA-ACK von Actor %d empfangen\n", msg->header.actorId);

  // Actor ist bereit für OTA
}

void Controller::authenticateActor(uint8_t actorId) {
  const DiscoveredActor* actor = g_discovery.findActorById(actorId);
  if (!actor || actor->authenticated) {
    return;
  }

  // Challenge senden
  ChallengeMessage msg = {};
  msg.header.msgType = MSG_CHALLENGE;
  msg.header.controllerId = m_controllerId;
  msg.header.actorId = actorId;
  msg.header.timestamp = millis();
  msg.header.nonce = g_security.generateNonce();

  g_security.generateChallenge(msg.challenge);

  g_espNow.sendUnicast(actor->mac, (uint8_t*)&msg, sizeof(msg));

  DEBUG_PRINTF("[Controller] Challenge an Actor %d gesendet\n", actorId);
}
