#pragma once

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
extern Preferences prefs;
// ================================================================
//  CONFIGURARE WIFI
//  Completeaza SSID-ul si parola retelei tale aici:
// ================================================================
const char* WIFI_SSID     = "TP-Link_98AE";   // <-- schimba aici
const char* WIFI_PASSWORD = "63536515";            // <-- schimba aici
// ================================================================

WebSocketsServer webSocket(81);

// Forward declarations pentru variabilele din main2.cpp
extern int     stareCurenta;
extern String  pinCorect;
extern bool    usaDeblocata;
extern bool    alarmaActiva;
extern int     numarDetectiiPIR;
extern int     numarDeschideri;
extern int     numarMiscariArmat;
extern int     greseliCountConsecutiv;
extern bool    pirAfisatPeEcran;
extern bool    reedStare;
// Forward declarations pentru functiile din main2.cpp
extern void armeazaSistem();
extern void dezarmeazaSistem();
extern void declansaAlarma(const char* motiv);
extern void tranzitieSpre(int stareNoua);
extern void actualizeazaOLED();

// Stari sistem (trebuie sa fie identice cu cele din main2.cpp)
#define STARE_DEZARMAT    0
#define STARE_ARMAT       1
#define STARE_ENTRY_DELAY 2
#define STARE_PIN_ENTRY   3
#define STARE_ALARMA      4
#define STARE_BLOCAT      5
#define STARE_APEL_112    6

// ────────────────────────────────────────────────────────────────
// Returneaza numele starii curente ca string
// ────────────────────────────────────────────────────────────────
String numeStare(int stare) {
    switch (stare) {
        case STARE_DEZARMAT:    return "DEZARMAT";
        case STARE_ARMAT:       return "ARMAT";
        case STARE_ENTRY_DELAY: return "ENTRY_DELAY";
        case STARE_PIN_ENTRY:   return "PIN_ENTRY";
        case STARE_ALARMA:      return "ALARMA";
        case STARE_BLOCAT:      return "BLOCAT";
        case STARE_APEL_112:    return "APEL_112";
        default:                return "NECUNOSCUT";
    }
}

// ────────────────────────────────────────────────────────────────
// Trimite statusul complet al sistemului la toti clientii conectati
// ────────────────────────────────────────────────────────────────
void sendStatusWeb() {
    StaticJsonDocument<256> doc;
    doc["type"]            = "status";
    doc["stare"]           = numeStare(stareCurenta);
    doc["pir"]             = pirAfisatPeEcran;
    doc["reed"]            = reedStare;
    doc["usa_deblocata"]   = usaDeblocata;
    doc["alarma_activa"]   = alarmaActiva;
    doc["miscari_armat"]   = numarMiscariArmat;
    doc["total_pir"]       = numarDetectiiPIR;
    doc["total_reed"]      = numarDeschideri;
    doc["greseli"]         = greseliCountConsecutiv;

    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);
}

// ────────────────────────────────────────────────────────────────
// Trimite un mesaj de notificare (toast) catre aplicatie
// ────────────────────────────────────────────────────────────────
void sendNotificare(const char* mesaj, const char* tip = "info") {
    StaticJsonDocument<128> doc;
    doc["type"]   = "notificare";
    doc["mesaj"]  = mesaj;
    doc["tip"]    = tip;  // "info", "warning", "error", "success"

    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);
}

void sendIstoricAlarma(const char* motiv) {
    StaticJsonDocument<128> doc;
    doc["type"]  = "alarma_log";
    doc["motiv"] = motiv;
    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);
}

// ────────────────────────────────────────────────────────────────
// Proceseaza comenzile primite de la aplicatia web
// ────────────────────────────────────────────────────────────────
void proceseazaComanda(const String& payload) {
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        Serial.print("JSON invalid: ");
        Serial.println(err.c_str());
        return;
    }

    String comanda = doc["cmd"].as<String>();
    Serial.print("Comanda web: ");
    Serial.println(comanda);

    if (comanda == "armeaza") {
        if (stareCurenta == STARE_DEZARMAT) {
            armeazaSistem();
            sendNotificare("Sistem armat din aplicatie", "success");
        } else {
            sendNotificare("Nu se poate arma acum", "warning");
        }
    }
    else if (comanda == "dezarmeaza") {
        dezarmeazaSistem();
        sendNotificare("Sistem dezarmat din aplicatie", "success");
    }
    else if (comanda == "schimba_pin") {
        String pinNou = doc["pin"].as<String>();
        if (pinNou.length() == 4) {
            bool valid = true;
            for (char c : pinNou) {
                if (c < '0' || c > '9') { valid = false; break; }
            }
            if (valid) {
                pinCorect = pinNou;
                prefs.putString("pin",pinCorect);
                Serial.print("PIN schimbat din app: ****");
                sendNotificare("PIN actualizat cu succes", "success");
                actualizeazaOLED();
            } else {
                sendNotificare("PIN invalid — doar cifre!", "error");
            }
        } else {
            sendNotificare("PIN trebuie sa aiba exact 4 cifre", "error");
        }
    }
    else if (comanda == "confirma_ok") {
        // Utilizatorul confirma ca nu e intrus — reseteaza counterul PIR
        if (stareCurenta == STARE_ARMAT) {
            numarMiscariArmat = 0;
            Serial.println("Counter PIR resetat de utilizator");
            sendNotificare("Counter miscari resetat", "success");
            sendStatusWeb();
        } else {
            sendNotificare("Confirmare disponibila doar in stare ARMAT", "warning");
        }
    }
    else if (comanda == "opreste_alarma") {
        if (stareCurenta == STARE_ALARMA || stareCurenta == STARE_BLOCAT || stareCurenta == STARE_APEL_112) {
            dezarmeazaSistem();
            sendNotificare("Alarma oprita din aplicatie", "success");
        } else {
            sendNotificare("Nu exista alarma activa", "warning");
        }
    }
    else {
        Serial.print("Comanda necunoscuta: ");
        Serial.println(comanda);
    }

    sendStatusWeb();
}

// ────────────────────────────────────────────────────────────────
// Callback WebSocket — apelat la orice eveniment de conexiune
// ────────────────────────────────────────────────────────────────
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.printf("Client WebSocket conectat: #%u\n", num);
            sendStatusWeb();  // trimite status imediat la conectare
            break;

        case WStype_DISCONNECTED:
            Serial.printf("Client WebSocket deconectat: #%u\n", num);
            break;

        case WStype_TEXT:
            proceseazaComanda(String((char*)payload));
            break;

        default:
            break;
    }
}

// ────────────────────────────────────────────────────────────────
// Initializare WiFi + WebSocket — apelata din setup()
// ────────────────────────────────────────────────────────────────
void setupWebServer() {
    Serial.println("Conectare WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int incercari = 0;
    while (WiFi.status() != WL_CONNECTED && incercari < 20) {
        delay(500);
        Serial.print(".");
        incercari++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("=== WiFi CONECTAT ===");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        Serial.println("Deschide dashboard.html si introdu IP-ul de mai sus");
        Serial.println("=====================");
    } else {
        Serial.println();
        Serial.println("EROARE: WiFi nu s-a conectat! Verifica SSID si parola.");
        Serial.println("Sistemul continua fara WiFi.");
    }

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("WebSocket pornit pe portul 81");
}

// ────────────────────────────────────────────────────────────────
// Trebuie apelata in loop() pentru a procesa mesajele WebSocket
// ────────────────────────────────────────────────────────────────
void handleWebServer() {
    webSocket.loop();
}