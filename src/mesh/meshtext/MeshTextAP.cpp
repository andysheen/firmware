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

    WiFi.disconnect(true, true);
    delay(100);

    if (!WiFi.mode(WIFI_AP)) return false;
    if (!WiFi.softAP("meshtext")) return false;

    IPAddress ip = WiFi.softAPIP();
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