#include "MeshTextAP.h"

#include <WiFi.h>
#include <ESPmDNS.h>

namespace meshtext {

static bool apActive = false;

bool startEditPagesAP()
{
    if (apActive) {
        return true;
    }

    // Clean slate
    WiFi.disconnect(true, true);
    delay(100);

    WiFi.mode(WIFI_AP);

    const char *ssid = "meshtext";
    const char *password = "";

    bool ok = WiFi.softAP(ssid);
    if (!ok) {
        return false;
    }

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("MeshText AP started: SSID=%s IP=%s\n", ssid, ip.toString().c_str());

    // Optional: try mDNS too
    MDNS.end();
    if (MDNS.begin("meshtext")) {
        Serial.println("MeshText AP mDNS started: http://meshtext.local/meshtext");
    }

    apActive = true;
    return true;
}

bool isEditPagesAPActive()
{
    return apActive;
}

}