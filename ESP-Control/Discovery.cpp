#include "Discovery.h"
#include "ESPNowComm.h"
#include <string.h>

Discovery g_discovery;

Discovery::Discovery() : m_actorCount(0), m_lastBeaconTime(0) {
  memset(m_actors, 0, sizeof(m_actors));
}

void Discovery::actorSendBeacon(uint8_t actorId, bool relayState) {
  uint32_t now = millis();

  // Nur alle BEACON_INTERVAL_MS senden
  if (now - m_lastBeaconTime < BEACON_INTERVAL_MS) {
    return;
  }

  m_lastBeaconTime = now;

  // Beacon-Nachricht erstellen
  BeaconMessage beacon = {};
  beacon.header.msgType = MSG_BEACON;
  beacon.header.actorId = actorId;
  beacon.header.timestamp = now;
  beacon.header.nonce = g_security.generateNonce();

  beacon.actorId = actorId;
  beacon.relayState = relayState;
  beacon.rssi = WiFi.RSSI();

  // Broadcast senden
  g_espNow.sendBroadcast((uint8_t*)&beacon, sizeof(beacon));

  DEBUG_PRINTF("[Discovery] Beacon gesendet (ActorID=%d, Relay=%d)\n", actorId, relayState);
}

void Discovery::controllerUpdate() {
  uint32_t now = millis();

  // Timeout für Actoren prüfen
  for (int i = 0; i < m_actorCount; i++) {
    if (now - m_actors[i].lastSeen > DISCOVERY_TIMEOUT_MS) {
      // Actor ist nicht mehr in Reichweite
      DEBUG_PRINTF("[Discovery] Actor %d nicht mehr in Reichweite\n", m_actors[i].actorId);

      // Actor aus Liste entfernen
      for (int j = i; j < m_actorCount - 1; j++) {
        m_actors[j] = m_actors[j + 1];
      }
      m_actorCount--;
      i--;
    }
  }
}

int Discovery::getDiscoveredActorCount() const {
  return m_actorCount;
}

const DiscoveredActor* Discovery::getDiscoveredActor(int index) const {
  if (index < 0 || index >= m_actorCount) {
    return nullptr;
  }
  return &m_actors[index];
}

const DiscoveredActor* Discovery::findActorById(uint8_t actorId) const {
  for (int i = 0; i < m_actorCount; i++) {
    if (m_actors[i].actorId == actorId) {
      return &m_actors[i];
    }
  }
  return nullptr;
}

void Discovery::registerActor(uint8_t actorId, const uint8_t* mac, bool relayState, int8_t rssi) {
  // Prüfen ob Actor bereits bekannt
  for (int i = 0; i < m_actorCount; i++) {
    if (m_actors[i].actorId == actorId) {
      // Update
      m_actors[i].relayState = relayState;
      m_actors[i].rssi = rssi;
      m_actors[i].lastSeen = millis();
      memcpy(m_actors[i].mac, mac, 6);
      return;
    }
  }

  // Neuer Actor
  if (m_actorCount >= MAX_ACTORS) {
    DEBUG_PRINTLN("[Discovery] Actor-Liste voll");
    return;
  }

  m_actors[m_actorCount].actorId = actorId;
  memcpy(m_actors[m_actorCount].mac, mac, 6);
  m_actors[m_actorCount].relayState = relayState;
  m_actors[m_actorCount].rssi = rssi;
  m_actors[m_actorCount].lastSeen = millis();
  m_actors[m_actorCount].authenticated = false;
  memset(m_actors[m_actorCount].sessionKey, 0, 32);

  m_actorCount++;

  DEBUG_PRINTF("[Discovery] Neuer Actor entdeckt: ID=%d, MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
               actorId, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void Discovery::markActorAuthenticated(uint8_t actorId, const uint8_t* sessionKey) {
  for (int i = 0; i < m_actorCount; i++) {
    if (m_actors[i].actorId == actorId) {
      m_actors[i].authenticated = true;
      memcpy(m_actors[i].sessionKey, sessionKey, 32);
      DEBUG_PRINTF("[Discovery] Actor %d authentifiziert\n", actorId);
      return;
    }
  }
}

void Discovery::clearActors() {
  m_actorCount = 0;
  memset(m_actors, 0, sizeof(m_actors));
  DEBUG_PRINTLN("[Discovery] Actor-Liste geleert");
}
