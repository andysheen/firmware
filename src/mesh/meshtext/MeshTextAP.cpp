#include "MeshTextAP.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include "mesh/http/WebServer.h"
//#include "mesh/http/API.h"

namespace meshtext {

static bool apActive = false;

bool startEditPagesAP()
{
    if (apActive) return true;

    WiFi.disconnect(false, false);
    delay(100);

    if (!WiFi.mode(WIFI_AP)) return false;
    if (!WiFi.softAP("meshtext")) return false;

    IPAddress ip = WiFi.softAPIP();
    delay(100);
    Serial.printf("MeshText AP started: %s\n", ip.toString().c_str());
    initHttpOnlyWebServer();
    apActive = true;
    return true;
}

bool isEditPagesAPActive()
{
    return apActive;
}

}
