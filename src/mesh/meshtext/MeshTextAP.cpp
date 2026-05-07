#include "MeshTextAP.h"

#include "configuration.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "mesh/http/WebServer.h"
//#include "mesh/http/API.h"

#if defined(ARCH_ESP32) && !CONFIG_IDF_TARGET_ESP32S2
#include "main.h"
#include <Preferences.h>
#endif

namespace meshtext {

static bool apActive = false;

static bool saveBootRequest()
{
#if defined(ARCH_ESP32) && !CONFIG_IDF_TARGET_ESP32S2
    Preferences prefs;
    if (!prefs.begin("MeshText", false))
        return false;

    bool ok = prefs.putBool("edit_ap_boot", true) == sizeof(bool);
    prefs.end();
    return ok;
#else
    return false;
#endif
}

static bool consumeBootRequest()
{
#if defined(ARCH_ESP32) && !CONFIG_IDF_TARGET_ESP32S2
    Preferences prefs;
    if (!prefs.begin("MeshText", false))
        return false;

    bool requested = prefs.getBool("edit_ap_boot", false);
    if (requested) {
        prefs.remove("edit_ap_boot");
    }
    prefs.end();
    return requested;
#else
    return false;
#endif
}

static bool startEditPagesAPNow()
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

EditPagesAPStartResult startEditPagesAP()
{
#if defined(ARCH_ESP32) && !CONFIG_IDF_TARGET_ESP32S2
    if (!apActive && nimbleBluetooth && nimbleBluetooth->isActive()) {
        Serial.println("MeshText: rebooting into editor AP before Bluetooth starts");
        if (!saveBootRequest())
            return EditPagesAPStartResult::Failed;

        rebootAtMsec = millis() + 1000;
        return EditPagesAPStartResult::Rebooting;
    }
#endif

    return startEditPagesAPNow() ? EditPagesAPStartResult::Started : EditPagesAPStartResult::Failed;
}

bool startEditPagesAPIfRequested()
{
    if (!consumeBootRequest())
        return false;

    Serial.println("MeshText: boot request found, starting Meshtext editor AP");
    return startEditPagesAPNow();
}

bool isEditPagesAPActive()
{
    return apActive;
}

}
