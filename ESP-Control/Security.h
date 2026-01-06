#ifndef SECURITY_H
#define SECURITY_H

#include "Config.h"
#include <mbedtls/md.h>
#include <mbedtls/aes.h>

// ============================================================================
// SICHERHEITS-LAYER MIT CHALLENGE-RESPONSE (OHNE NVS)
// ============================================================================

class Security {
public:
  Security();

  // Initialisierung mit Shared Secret aus ESP_Code_Key.h
  void begin(const char* sharedSecret);

  // === Actor-Funktionen ===
  // Generiert einen neuen Session-Key (bei Boot)
  void generateSessionKey();

  // Beantwortet eine Challenge mit Response
  bool generateResponse(const uint8_t* challenge, uint8_t* response);

  // === Controller-Funktionen ===
  // Generiert eine neue Challenge
  void generateChallenge(uint8_t* challenge);

  // Validiert eine Response
  bool validateResponse(const uint8_t* challenge, const uint8_t* response,
                        const uint8_t* actorSessionKey);

  // === Beide ===
  // Generiert HMAC für Command (gegen Replay)
  bool generateCommandHMAC(const MessageHeader* header, uint8_t* hmac);

  // Validiert HMAC für Command
  bool validateCommandHMAC(const MessageHeader* header, const uint8_t* hmac);

  // Prüft Nonce (gegen Replay) - verwendet In-Memory-Cache
  bool validateNonce(uint32_t nonce, uint8_t deviceId);

  // Generiert Nonce
  uint32_t generateNonce();

  // Session-Key (für Actor)
  void getSessionKey(uint8_t* outKey, size_t len);

private:
  char m_sharedSecret[64];
  uint8_t m_sessionKey[32];

  // Nonce-Cache (In-Memory, gegen Replay)
  struct NonceEntry {
    uint8_t deviceId;
    uint32_t nonce;
    uint32_t timestamp;
  };

  static const int MAX_NONCE_CACHE = 50;
  NonceEntry m_nonceCache[MAX_NONCE_CACHE];
  int m_nonceCacheIndex;

  // HMAC-SHA256 Helper
  bool computeHMAC(const uint8_t* key, size_t keyLen,
                   const uint8_t* data, size_t dataLen,
                   uint8_t* output);
};

extern Security g_security;

#endif // SECURITY_H
