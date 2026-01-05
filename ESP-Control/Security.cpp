#include "Security.h"
#include <esp_random.h>
#include <string.h>

Security g_security;

Security::Security() : m_nonceCacheIndex(0) {
  memset(m_sharedSecret, 0, sizeof(m_sharedSecret));
  memset(m_sessionKey, 0, sizeof(m_sessionKey));
  memset(m_nonceCache, 0, sizeof(m_nonceCache));
}

void Security::begin(const char* sharedSecret) {
  strncpy(m_sharedSecret, sharedSecret, sizeof(m_sharedSecret) - 1);
  DEBUG_PRINTLN("[Security] Initialisiert");
}

void Security::generateSessionKey() {
  // Generiere zufälligen 256-bit Session-Key
  for (int i = 0; i < 32; i++) {
    m_sessionKey[i] = esp_random() & 0xFF;
  }
  DEBUG_PRINTLN("[Security] Neuer Session-Key generiert");
}

bool Security::generateResponse(const uint8_t* challenge, uint8_t* response) {
  // Response = HMAC-SHA256(SessionKey || SharedSecret, Challenge)
  uint8_t combinedKey[64];
  memcpy(combinedKey, m_sessionKey, 32);
  memcpy(combinedKey + 32, m_sharedSecret, 32);

  return computeHMAC(combinedKey, 64, challenge, 16, response);
}

void Security::generateChallenge(uint8_t* challenge) {
  // 16 Byte zufällige Challenge
  for (int i = 0; i < 16; i++) {
    challenge[i] = esp_random() & 0xFF;
  }
}

bool Security::validateResponse(const uint8_t* challenge, const uint8_t* response,
                                 const uint8_t* actorSessionKey) {
  // Berechne erwartete Response
  uint8_t expectedResponse[32];
  uint8_t combinedKey[64];

  memcpy(combinedKey, actorSessionKey, 32);
  memcpy(combinedKey + 32, m_sharedSecret, 32);

  if (!computeHMAC(combinedKey, 64, challenge, 16, expectedResponse)) {
    return false;
  }

  // Vergleiche
  return memcmp(expectedResponse, response, 32) == 0;
}

bool Security::generateCommandHMAC(const MessageHeader* header, uint8_t* hmac) {
  // HMAC über den gesamten Header
  return computeHMAC((uint8_t*)m_sharedSecret, strlen(m_sharedSecret),
                     (uint8_t*)header, sizeof(MessageHeader), hmac);
}

bool Security::validateCommandHMAC(const MessageHeader* header, const uint8_t* hmac) {
  uint8_t expectedHMAC[32];

  if (!generateCommandHMAC(header, expectedHMAC)) {
    return false;
  }

  return memcmp(expectedHMAC, hmac, 32) == 0;
}

uint32_t Security::generateNonce() {
  return esp_random();
}

bool Security::validateNonce(uint32_t nonce, uint8_t deviceId) {
  uint32_t now = millis();

  // Prüfe ob Nonce bereits im Cache
  for (int i = 0; i < MAX_NONCE_CACHE; i++) {
    if (m_nonceCache[i].deviceId == deviceId &&
        m_nonceCache[i].nonce == nonce) {
      // Replay-Angriff erkannt
      DEBUG_PRINTF("[Security] Replay erkannt! Nonce=%lu, DeviceID=%d\n", nonce, deviceId);
      return false;
    }
  }

  // Nonce in Cache eintragen
  m_nonceCache[m_nonceCacheIndex].deviceId = deviceId;
  m_nonceCache[m_nonceCacheIndex].nonce = nonce;
  m_nonceCache[m_nonceCacheIndex].timestamp = now;

  m_nonceCacheIndex = (m_nonceCacheIndex + 1) % MAX_NONCE_CACHE;

  // Alte Einträge aus Cache entfernen (> 10 Sekunden)
  for (int i = 0; i < MAX_NONCE_CACHE; i++) {
    if (now - m_nonceCache[i].timestamp > 10000) {
      m_nonceCache[i].deviceId = 0xFF;
      m_nonceCache[i].nonce = 0;
      m_nonceCache[i].timestamp = 0;
    }
  }

  return true;
}

void Security::getSessionKey(uint8_t* outKey, size_t len) {
  size_t copyLen = (len < 32) ? len : 32;
  memcpy(outKey, m_sessionKey, copyLen);
}

bool Security::computeHMAC(const uint8_t* key, size_t keyLen,
                           const uint8_t* data, size_t dataLen,
                           uint8_t* output) {
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);

  if (mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  if (mbedtls_md_hmac_starts(&ctx, key, keyLen) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  if (mbedtls_md_hmac_update(&ctx, data, dataLen) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  if (mbedtls_md_hmac_finish(&ctx, output) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  mbedtls_md_free(&ctx);
  return true;
}
