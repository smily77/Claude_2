#ifndef ACTOR_H
#define ACTOR_H

#include "Config.h"

// ============================================================================
// ACTOR-LOGIK
// ============================================================================

class Actor {
public:
  Actor();

  // Initialisierung (liest ESP_Code_Conf.h)
  void begin(uint8_t actorId, uint8_t relayPin,
             const EnablePinConfig& enablePin,
             const IndicatorConfig indicators[3]);

  // Haupt-Update (regelmäßig aufrufen)
  void update();

  // Relais steuern
  void setRelay(bool state);
  void toggleRelay();
  bool getRelayState() const { return m_relayState; }

  // Enable-Pin Status (Actor sichtbar?)
  bool isEnabled() const;

  // Indikatoren aktualisieren
  void updateIndicators();

  // Nachrichtenbehandlung
  void handleMessage(const uint8_t* mac, const uint8_t* data, int len);

private:
  uint8_t m_actorId;
  uint8_t m_relayPin;
  bool m_relayState;

  EnablePinConfig m_enablePin;
  IndicatorConfig m_indicators[3];

  uint32_t m_lastBeaconTime;

  // Nachrichtenhandler
  void handleRelayToggle(const uint8_t* mac, const RelayToggleMessage* msg);
  void handleOTAPrepare(const uint8_t* mac, const OTAPrepareMessage* msg);
  void handleChallenge(const uint8_t* mac, const ChallengeMessage* msg);

  // Indikator-Steuerung
  void setIndicator(int index, bool state);
};

extern Actor g_actor;

#endif // ACTOR_H
