/*
 * I2CSensorBridge.h
 * Multi-Sensor I2C Communication Library for ESP32
 * 
 * Ermöglicht die Übertragung beliebiger Struct-Daten zwischen
 * mehreren ESP32-Slaves und einem Master über I2C
 * 
 * Author: Stefan
 * Version: 1.0.0
 */

#ifndef I2C_SENSOR_BRIDGE_H
#define I2C_SENSOR_BRIDGE_H

#include <Arduino.h>
#include <Wire.h>

// ==================== KONFIGURATION ====================
#define I2C_BRIDGE_MAX_STRUCTS 8        // Max Anzahl Structs pro Device
#define I2C_BRIDGE_BUFFER_SIZE 128       // Max Größe eines Structs
#define I2C_BRIDGE_DEBUG 0               // Debug-Ausgaben (0=aus, 1=an)

// I2C Kommando-Bytes
#define CMD_GET_STATUS      0x01  // Status-Byte abfragen (welche Structs sind neu)
#define CMD_READ_STRUCT     0x02  // Struct-Daten lesen
#define CMD_CLEAR_FLAG      0x03  // New-Data Flag löschen
#define CMD_GET_INFO        0x04  // Struct-Info abfragen (Size, Version)
#define CMD_PING            0x05  // Verbindungstest
#define CMD_GET_COUNT       0x06  // Anzahl registrierter Structs

// Fehler-Codes
#define I2C_BRIDGE_OK           0
#define I2C_BRIDGE_ERR_FULL    -1
#define I2C_BRIDGE_ERR_NOTFOUND -2
#define I2C_BRIDGE_ERR_SIZE    -3
#define I2C_BRIDGE_ERR_COMM    -4

// ==================== HAUPT-KLASSE ====================

class I2CSensorBridge {
private:
    // Struct-Registry Eintrag
    struct StructEntry {
        uint8_t id;                             // Struct ID (0-7)
        uint16_t size;                          // Größe in Bytes
        uint8_t version;                        // Version für Kompatibilität
        void* dataPtr;                           // Pointer zu den Daten
        bool hasNewData;                         // Flag für neue Daten
        unsigned long lastUpdate;                // Timestamp letztes Update
        char name[16];                           // Debug-Name
        bool inUse;                             // Slot belegt?
    };
    
    // Member-Variablen
    StructEntry registry[I2C_BRIDGE_MAX_STRUCTS];  // Struct-Registry
    uint8_t registryCount;                          // Anzahl registrierter Structs
    
    bool isMaster;                                  // Master oder Slave Mode
    uint8_t deviceAddress;                           // I2C Adresse (nur Slave)
    TwoWire* wireInterface;                          // Wire Interface Pointer
    
    // Kommunikations-Buffer
    uint8_t txBuffer[I2C_BRIDGE_BUFFER_SIZE];       // Sende-Buffer
    uint8_t rxBuffer[I2C_BRIDGE_BUFFER_SIZE];       // Empfangs-Buffer
    
    // Slave-spezifisch
    volatile uint8_t currentCommand;                 // Aktueller Befehl
    volatile uint8_t currentStructId;                // Angeforderter Struct
    volatile bool requestPending;                    // Request ausstehend
    
    // Singleton für Wire Callbacks
    static I2CSensorBridge* activeInstance;
    
public:
    // ==================== KONSTRUKTOR/SETUP ====================
    
    I2CSensorBridge(TwoWire& wire = Wire) : wireInterface(&wire) {
        registryCount = 0;
        isMaster = true;
        deviceAddress = 0;
        currentCommand = 0;
        currentStructId = 0;
        requestPending = false;
        
        // Registry initialisieren
        for (int i = 0; i < I2C_BRIDGE_MAX_STRUCTS; i++) {
            registry[i].inUse = false;
            registry[i].hasNewData = false;
        }
    }
    
    /**
     * Initialisierung als Master
     * @param sda SDA Pin (optional, -1 für default)
     * @param scl SCL Pin (optional, -1 für default)
     * @param frequency I2C Frequenz in Hz (optional)
     */
    void beginMaster(int8_t sda = -1, int8_t scl = -1, uint32_t frequency = 100000) {
        isMaster = true;
        deviceAddress = 0;
        
        if (sda >= 0 && scl >= 0) {
            wireInterface->begin(sda, scl, frequency);
        } else {
            wireInterface->begin();
        }
        
        #if I2C_BRIDGE_DEBUG
        Serial.println("[I2C Bridge] Master mode initialized");
        #endif
    }
    
    /**
     * Initialisierung als Slave
     * @param address I2C Slave-Adresse (7-bit)
     * @param sda SDA Pin (optional)
     * @param scl SCL Pin (optional)
     */
    void beginSlave(uint8_t address, int8_t sda = -1, int8_t scl = -1) {
        isMaster = false;
        deviceAddress = address;
        
        // Singleton setzen für Callbacks
        activeInstance = this;
        
        if (sda >= 0 && scl >= 0) {
            wireInterface->begin(address, sda, scl);
        } else {
            wireInterface->begin(address);
        }
        
        // Callbacks registrieren
        wireInterface->onRequest(onRequestStatic);
        wireInterface->onReceive(onReceiveStatic);
        
        #if I2C_BRIDGE_DEBUG
        Serial.printf("[I2C Bridge] Slave mode initialized on address 0x%02X\n", address);
        #endif
    }
    
    // ==================== STRUCT REGISTRIERUNG ====================
    
    /**
     * Struct registrieren (Master & Slave)
     * @param id Eindeutige ID (0-7)
     * @param dataPtr Pointer zum Struct
     * @param size Größe des Structs
     * @param version Version für Kompatibilitätsprüfung
     * @param name Debug-Name (optional)
     * @return Error code (0 = success)
     */
    template<typename T>
    int8_t registerStruct(uint8_t id, T* dataPtr, uint8_t version = 1, const char* name = "") {
        if (id >= I2C_BRIDGE_MAX_STRUCTS) {
            return I2C_BRIDGE_ERR_FULL;
        }
        
        if (sizeof(T) > I2C_BRIDGE_BUFFER_SIZE) {
            return I2C_BRIDGE_ERR_SIZE;
        }
        
        registry[id].id = id;
        registry[id].size = sizeof(T);
        registry[id].version = version;
        registry[id].dataPtr = (void*)dataPtr;
        registry[id].hasNewData = false;
        registry[id].lastUpdate = 0;
        registry[id].inUse = true;
        
        strncpy(registry[id].name, name, 15);
        registry[id].name[15] = '\0';
        
        if (id >= registryCount) {
            registryCount = id + 1;
        }
        
        #if I2C_BRIDGE_DEBUG
        Serial.printf("[I2C Bridge] Registered struct ID=%d, size=%d, name='%s'\n", 
                     id, sizeof(T), name);
        #endif
        
        return I2C_BRIDGE_OK;
    }
    
    // ==================== SLAVE FUNKTIONEN ====================
    
    /**
     * Struct-Daten aktualisieren (nur Slave)
     * @param id Struct ID
     * @param data Neue Daten
     * @return true bei Erfolg
     */
    template<typename T>
    bool updateStruct(uint8_t id, const T& data) {
        if (isMaster || id >= I2C_BRIDGE_MAX_STRUCTS) {
            return false;
        }
        
        if (!registry[id].inUse || registry[id].size != sizeof(T)) {
            return false;
        }
        
        // Daten kopieren
        memcpy(registry[id].dataPtr, &data, sizeof(T));
        registry[id].hasNewData = true;
        registry[id].lastUpdate = millis();
        
        #if I2C_BRIDGE_DEBUG
        Serial.printf("[I2C Bridge] Updated struct ID=%d\n", id);
        #endif
        
        return true;
    }
    
    /**
     * Status-Byte generieren (Bitmap der Structs mit neuen Daten)
     */
    uint8_t getStatusByte() {
        uint8_t status = 0;
        for (uint8_t i = 0; i < 8 && i < registryCount; i++) {
            if (registry[i].inUse && registry[i].hasNewData) {
                status |= (1 << i);
            }
        }
        return status;
    }
    
    // ==================== MASTER FUNKTIONEN ====================
    
    /**
     * Slave-Gerät pingen
     * @param slaveAddress I2C Adresse
     * @return true wenn Gerät antwortet
     */
    bool ping(uint8_t slaveAddress) {
        if (!isMaster) return false;
        
        wireInterface->beginTransmission(slaveAddress);
        uint8_t error = wireInterface->endTransmission();
        
        return (error == 0);
    }
    
    /**
     * Status von Slave abfragen (welche Structs haben neue Daten)
     * @param slaveAddress I2C Adresse
     * @return Status-Byte (Bitmap)
     */
    uint8_t checkNewData(uint8_t slaveAddress) {
        if (!isMaster) return 0;
        
        // Status anfordern
        wireInterface->beginTransmission(slaveAddress);
        wireInterface->write(CMD_GET_STATUS);
        if (wireInterface->endTransmission() != 0) {
            return 0; // Kommunikationsfehler
        }
        
        // Antwort lesen
        wireInterface->requestFrom(slaveAddress, (uint8_t)1);
        
        if (wireInterface->available()) {
            uint8_t status = wireInterface->read();
            
            #if I2C_BRIDGE_DEBUG
            Serial.printf("[I2C Bridge] Status from 0x%02X: 0x%02X\n", 
                         slaveAddress, status);
            #endif
            
            return status;
        }
        
        return 0;
    }
    
    /**
     * Struct von Slave lesen
     * @param slaveAddress I2C Adresse
     * @param structId Struct ID
     * @param buffer Buffer für empfangene Daten
     * @return true bei Erfolg
     */
    template<typename T>
    bool readStruct(uint8_t slaveAddress, uint8_t structId, T& buffer) {
        if (!isMaster) return false;
        
        size_t expectedSize = sizeof(T);
        
        // Sicherstellen dass der Buffer groß genug ist
        if (expectedSize > I2C_BRIDGE_BUFFER_SIZE) {
            return false;
        }
        
        // Struct anfordern
        wireInterface->beginTransmission(slaveAddress);
        wireInterface->write(CMD_READ_STRUCT);
        wireInterface->write(structId);
        if (wireInterface->endTransmission() != 0) {
            #if I2C_BRIDGE_DEBUG
            Serial.println("[I2C Bridge] Communication error");
            #endif
            return false;
        }
        
        // Kurze Pause für Slave-Verarbeitung
        delayMicroseconds(100);
        
        // Daten empfangen (in Chunks wenn nötig)
        size_t totalReceived = 0;
        uint8_t* ptr = (uint8_t*)&buffer;
        
        while (totalReceived < expectedSize) {
            size_t chunkSize = min((size_t)32, expectedSize - totalReceived);
            
            size_t received = wireInterface->requestFrom(slaveAddress, (uint8_t)chunkSize);
            
            if (received == 0) {
                #if I2C_BRIDGE_DEBUG
                Serial.println("[I2C Bridge] No data received");
                #endif
                return false;
            }
            
            for (size_t i = 0; i < received && totalReceived < expectedSize; i++) {
                ptr[totalReceived++] = wireInterface->read();
            }
        }
        
        // Flag beim Slave zurücksetzen
        wireInterface->beginTransmission(slaveAddress);
        wireInterface->write(CMD_CLEAR_FLAG);
        wireInterface->write(structId);
        wireInterface->endTransmission();
        
        #if I2C_BRIDGE_DEBUG
        Serial.printf("[I2C Bridge] Read %d bytes from struct ID=%d\n", 
                     totalReceived, structId);
        #endif
        
        return (totalReceived == expectedSize);
    }
    
    /**
     * Anzahl registrierter Structs beim Slave abfragen
     */
    uint8_t getSlaveStructCount(uint8_t slaveAddress) {
        if (!isMaster) return 0;
        
        wireInterface->beginTransmission(slaveAddress);
        wireInterface->write(CMD_GET_COUNT);
        if (wireInterface->endTransmission() != 0) {
            return 0;
        }
        
        wireInterface->requestFrom(slaveAddress, (uint8_t)1);
        
        if (wireInterface->available()) {
            return wireInterface->read();
        }
        
        return 0;
    }
    
    // ==================== UTILITY FUNKTIONEN ====================
    
    /**
     * Anzahl registrierter Structs
     */
    uint8_t getStructCount() const {
        return registryCount;
    }
    
    /**
     * Prüfen ob Struct neue Daten hat (lokal)
     */
    bool hasNewData(uint8_t id) const {
        if (id >= I2C_BRIDGE_MAX_STRUCTS) return false;
        return registry[id].inUse && registry[id].hasNewData;
    }
    
    /**
     * New-Data Flag zurücksetzen (lokal)
     */
    void clearNewDataFlag(uint8_t id) {
        if (id < I2C_BRIDGE_MAX_STRUCTS && registry[id].inUse) {
            registry[id].hasNewData = false;
        }
    }
    
    /**
     * Zeitstempel des letzten Updates
     */
    unsigned long getLastUpdate(uint8_t id) const {
        if (id >= I2C_BRIDGE_MAX_STRUCTS || !registry[id].inUse) {
            return 0;
        }
        return registry[id].lastUpdate;
    }
    
private:
    // ==================== I2C SLAVE CALLBACKS ====================
    
    static void onRequestStatic() {
        if (activeInstance) {
            activeInstance->onRequest();
        }
    }
    
    static void onReceiveStatic(int bytes) {
        if (activeInstance) {
            activeInstance->onReceive(bytes);
        }
    }
    
    void onRequest() {
        #if I2C_BRIDGE_DEBUG
        Serial.printf("[I2C Bridge] onRequest: cmd=0x%02X\n", currentCommand);
        #endif
        
        switch (currentCommand) {
            case CMD_GET_STATUS: {
                uint8_t status = getStatusByte();
                wireInterface->write(status);
                break;
            }
            
            case CMD_READ_STRUCT: {
                if (currentStructId < I2C_BRIDGE_MAX_STRUCTS && 
                    registry[currentStructId].inUse) {
                    
                    // Daten in Chunks senden (ESP32 I2C Buffer Limit)
                    uint8_t* data = (uint8_t*)registry[currentStructId].dataPtr;
                    size_t size = registry[currentStructId].size;
                    size_t chunkSize = min(size, (size_t)32);
                    
                    wireInterface->write(data, chunkSize);
                }
                break;
            }
            
            case CMD_GET_COUNT: {
                wireInterface->write(registryCount);
                break;
            }
            
            case CMD_GET_INFO: {
                if (currentStructId < I2C_BRIDGE_MAX_STRUCTS && 
                    registry[currentStructId].inUse) {
                    // Info-Paket senden: [size_low, size_high, version]
                    wireInterface->write(registry[currentStructId].size & 0xFF);
                    wireInterface->write(registry[currentStructId].size >> 8);
                    wireInterface->write(registry[currentStructId].version);
                }
                break;
            }
            
            case CMD_PING: {
                wireInterface->write(0xAA); // Antwort-Byte
                break;
            }
        }
    }
    
    void onReceive(int bytes) {
        if (bytes < 1) return;
        
        currentCommand = wireInterface->read();
        
        #if I2C_BRIDGE_DEBUG
        Serial.printf("[I2C Bridge] onReceive: cmd=0x%02X, bytes=%d\n", 
                     currentCommand, bytes);
        #endif
        
        switch (currentCommand) {
            case CMD_READ_STRUCT:
            case CMD_GET_INFO:
                if (bytes >= 2) {
                    currentStructId = wireInterface->read();
                }
                break;
                
            case CMD_CLEAR_FLAG:
                if (bytes >= 2) {
                    uint8_t id = wireInterface->read();
                    if (id < I2C_BRIDGE_MAX_STRUCTS && registry[id].inUse) {
                        registry[id].hasNewData = false;
                    }
                }
                break;
        }
        
        // Restliche Bytes verwerfen
        while (wireInterface->available()) {
            wireInterface->read();
        }
    }
};

// Static Member initialisieren
I2CSensorBridge* I2CSensorBridge::activeInstance = nullptr;

#endif // I2C_SENSOR_BRIDGE_H
