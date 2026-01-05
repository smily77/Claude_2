#ifndef DISCOVERY_H
#define DISCOVERY_H

#include "Config.h"

// ============================================================================
// DISCOVERY-MECHANISMUS FÜR ACTOR-CONTROLLER-VERBINDUNG
// ============================================================================

// Informationen über einen entdeckten Actor
struct DiscoveredActor {
  uint8_t actorId;
  uint8_t mac[6];
  bool relayState;
  int8_t rssi;
  uint32_t lastSeen;
  bool authenticated;
  uint8_t sessionKey[32];
};

class Discovery {
public:
  Discovery();

  // Actor-Funktionen
  void actorSendBeacon(uint8_t actorId, bool relayState);

  // Controller-Funktionen
  void controllerUpdate();  // regelmäßig aufrufen
  int getDiscoveredActorCount() const;
  const DiscoveredActor* getDiscoveredActor(int index) const;
  const DiscoveredActor* findActorById(uint8_t actorId) const;

  // Actor als "entdeckt" registrieren (nach Beacon-Empfang)
  void registerActor(uint8_t actorId, const uint8_t* mac, bool relayState, int8_t rssi);

  // Actor-Authentifizierung abschließen
  void markActorAuthenticated(uint8_t actorId, const uint8_t* sessionKey);

  // Actor-Liste zurücksetzen
  void clearActors();

private:
  static const int MAX_ACTORS = 10;
  DiscoveredActor m_actors[MAX_ACTORS];
  int m_actorCount;

  uint32_t m_lastBeaconTime;
};

extern Discovery g_discovery;

#endif // DISCOVERY_H
