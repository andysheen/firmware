#include "MeshTextAP.h"

#include "configuration.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "NodeDB.h"
#include "mesh/http/WebServer.h"
//#include "mesh/http/API.h"

#if defined(ARCH_ESP32) && !CONFIG_IDF_TARGET_ESP32S2
#include "main.h"
#include <Preferences.h>
#endif

namespace meshtext {

static bool apActive = false;
static char apSSID[33] = "meshtext";

static const char *buildAPSSID()
{
    const char *shortName = owner.short_name[0] ? owner.short_name : "node";
    snprintf(apSSID, sizeof(apSSID), "meshtext-%s", shortName);
    apSSID[sizeof(apSSID) - 1] = '\0';
    return apSSID;
}

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

static void clearBootRequest()
{
#if defined(ARCH_ESP32) && !CONFIG_IDF_TARGET_ESP32S2
    Preferences prefs;
    if (!prefs.begin("MeshText", false))
        return;

    prefs.remove("edit_ap_boot");
    prefs.end();
#endif
}

static bool startEditPagesAPNow()
{
    if (apActive) return true;

    WiFi.disconnect(false, false);
    delay(100);

    const char *ssid = buildAPSSID();

    if (!WiFi.mode(WIFI_AP)) return false;
    if (!WiFi.softAP(ssid)) return false;

    IPAddress ip = WiFi.softAPIP();
    delay(100);
    Serial.printf("MeshText AP started: %s at %s\n", ssid, ip.toString().c_str());
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

void stopEditPagesAP()
{
    clearBootRequest();

#if defined(ARCH_ESP32)
    if (apActive || (WiFi.getMode() & WIFI_AP)) {
        Serial.println("MeshText: stopping editor AP");
        WiFi.softAPdisconnect(true);
        delay(100);
        WiFi.mode(WIFI_OFF);
        delay(100);
    }
#endif

    apActive = false;

#if defined(ARCH_ESP32) && !CONFIG_IDF_TARGET_ESP32S2
    if (!config.bluetooth.enabled) {
        Serial.println("MeshText: enabling Bluetooth after editor AP stopped");
        config.bluetooth.enabled = true;
        if (nodeDB) {
            nodeDB->saveToDisk(SEGMENT_CONFIG);
        }
    }

    setBluetoothEnable(true);

    if (nimbleBluetooth && nimbleBluetooth->isDeInit) {
        Serial.println("MeshText: Bluetooth was deinitialized, rebooting to restore it");
        rebootAtMsec = millis() + 1000;
    }
#endif
}

bool isEditPagesAPActive()
{
    return apActive;
}

const char *getEditPagesAPSSID()
{
    return buildAPSSID();
}

}
