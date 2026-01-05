#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "Config.h"

// ============================================================================
// CONTROLLER-LOGIK
// ============================================================================

class Controller {
public:
  Controller();

  // Initialisierung (liest ESP_Code_Conf.h)
  void begin(uint8_t controllerId,
             const uint8_t actorIds[], int actorCount,
             const ButtonConfig buttons[3],
             const IndicatorConfig leds[3],
             const DisplayConfig& display);

  // Haupt-Update (regelmäßig aufrufen)
  void update();

  // Taster-Logik
  void updateButtons();

  // LEDs/Display aktualisieren
  void updateIndicators();

  // Nachrichtenbehandlung
  void handleMessage(const uint8_t* mac, const uint8_t* data, int len);

  // OTA-Modus starten
  void startOTAMode();

private:
  uint8_t m_controllerId;
  uint8_t m_actorIds[10];
  int m_actorCount;

  ButtonConfig m_buttons[3];
  IndicatorConfig m_leds[3];
  DisplayConfig m_display;

  // Taster-Zustand
  struct ButtonState {
    bool lastState;
    uint32_t lastChangeTime;
    uint32_t pressStartTime;
    bool longPressTriggered;
  };
  ButtonState m_buttonStates[3];

  // Nachrichtenhandler
  void handleBeacon(const uint8_t* mac, const BeaconMessage* msg);
  void handleRelayState(const uint8_t* mac, const RelayStateMessage* msg);
  void handleResponse(const uint8_t* mac, const ResponseMessage* msg);
  void handleOTAAck(const uint8_t* mac, const OTAAckMessage* msg);

  // Taster-Aktionen
  void onButtonShortPress(int buttonIndex);
  void onButtonLongPress(int buttonIndex);

  // Actor-Steuerung
  void sendRelayToggle(uint8_t actorId);

  // Authentifizierung
  void authenticateActor(uint8_t actorId);

  // LED-Steuerung
  void setLED(int index, bool state);
};

extern Controller g_controller;

#endif // CONTROLLER_H
