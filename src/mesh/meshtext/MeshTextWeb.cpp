#include "MeshTextWeb.h"
#include "meshtext_editor_html_gz.h"
#include "NodeDB.h"
#include "mesh/meshtext/MeshTextStorage.h"
#include <ArduinoJson.h>
#include <FSCommon.h> 
#include <string>
#include <cstdlib>

using namespace httpsserver;

namespace meshtext {

struct MeshTextConfig {
    int category = 0;
};

static void ensureMeshTextDir() {
    if (!FSCom.exists("/meshtext")) {
        FSCom.mkdir("/meshtext");
    }
}

static bool loadMeshTextConfig(MeshTextConfig &cfg) {
    ensureMeshTextDir();

    File f = FSCom.open("/meshtext/config.json", "r");
    if (!f) {
        cfg.category = 0;
        return false;
    }

    String body;
    while (f.available()) {
        body += char(f.read());
    }
    f.close();

    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        cfg.category = 0;
        return false;
    }

    cfg.category = doc["category"] | 0;
    return true;
}

static bool saveMeshTextConfig(const MeshTextConfig &cfg) {
    ensureMeshTextDir();

    File f = FSCom.open("/meshtext/config.json", "w");
    if (!f) return false;

    JsonDocument doc;
    doc["category"] = cfg.category;

    String out;
    serializeJson(doc, out);
    size_t written = f.print(out);
    f.close();

    return written == out.length();
}

struct PageListEntry {
    uint8_t page_num;
    char title[16];
};


static void pagePath(uint8_t num, char *buf, size_t len) {
    snprintf(buf, len, "/pages/%03u.bin", num);
}

static uint8_t listPages(PageListEntry *list, uint8_t maxEntries) {
    File dir = FSCom.open("/pages");
    if (!dir || !dir.isDirectory()) return 0;

    uint8_t count = 0;
    File f = dir.openNextFile();

    while (f && count < maxEntries) {
        if (f.size() == sizeof(Page)) {
            Page p{};
            if (f.read(reinterpret_cast<uint8_t *>(&p), sizeof(Page)) == sizeof(Page)) {
                list[count].page_num = p.page_num;
                memcpy(list[count].title, p.title, sizeof(list[count].title));
                list[count].title[sizeof(list[count].title) - 1] = '\0';
                count++;
            }
        }
        f.close();
        f = dir.openNextFile();
    }

    dir.close();

    for (uint8_t i = 1; i < count; i++) {
        PageListEntry tmp = list[i];
        int j = i - 1;
        while (j >= 0 && list[j].page_num > tmp.page_num) {
            list[j + 1] = list[j];
            j--;
        }
        list[j + 1] = tmp;
    }

    return count;
}

static void handleEditor(HTTPRequest *req, HTTPResponse *res) {
    (void)req;

    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html; charset=utf-8");
    res->setHeader("Content-Encoding", "gzip");
    res->setHeader("Cache-Control", "no-cache");

    res->write(
        reinterpret_cast<const uint8_t *>(meshtext_editor_html_gz),
        meshtext_editor_html_gz_len
    );
}

static void handleConfig(HTTPRequest *req, HTTPResponse *res) {
    (void)req;

    MeshTextConfig mtcfg;
    loadMeshTextConfig(mtcfg);

    JsonDocument doc;
    doc["name"] = owner.short_name;
    doc["category"] = mtcfg.category;
    doc["node_id"] = "0000";  // keep your stub for now

    String out;
    serializeJson(doc, out);

    res->setStatusCode(200);
    res->setHeader("Content-Type", "application/json; charset=utf-8");
    res->print(out);
}

static void handlePages(HTTPRequest *req, HTTPResponse *res) {
    (void)req;

    PageListEntry list[MAX_PAGES];
    uint8_t count = listPages(list, MAX_PAGES);

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (uint8_t i = 0; i < count; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["page_num"] = list[i].page_num;
        obj["title"] = list[i].title;
    }

    String out;
    serializeJson(doc, out);

    res->setStatusCode(200);
    res->setHeader("Content-Type", "application/json; charset=utf-8");
    res->print(out);
}

static bool getPageNumFromRequest(HTTPRequest *req, uint8_t &pageNum) {
    ResourceParameters *params = req->getParams();
    if (!params) return false;

    std::string param;
    if (!params->getPathParameter(0, param)) return false;

    int num = atoi(param.c_str());
    if (num < 1 || num > 255) return false;

    pageNum = static_cast<uint8_t>(num);
    return true;
}

static void handlePage(HTTPRequest *req, HTTPResponse *res) {
    uint8_t pageNum = 0;
    if (!getPageNumFromRequest(req, pageNum)) {
        res->setStatusCode(400);
        res->setHeader("Content-Type", "text/plain");
        res->print("Invalid page");
        return;
    }

    Page p{};
    if (!loadPage(pageNum, p)) {
        res->setStatusCode(404);
        res->setHeader("Content-Type", "text/plain");
        res->print("Not found");
        return;
    }

    JsonDocument doc;
    doc["page_num"] = p.page_num;
    doc["flags"] = p.flags;
    doc["title"] = p.title;

    JsonArray cells = doc["cells"].to<JsonArray>();
    for (int i = 0; i < PAGE_CELLS; i++) {
        cells.add(p.cells[i]);
    }

    String out;
    serializeJson(doc, out);

    res->setStatusCode(200);
    res->setHeader("Content-Type", "application/json; charset=utf-8");
    res->print(out);
}

static bool readRequestBody(HTTPRequest *req, String &body) {
    body = "";

    uint8_t buffer[256];
    size_t bytesRead = 0;

    while ((bytesRead = req->readBytes(buffer, sizeof(buffer))) > 0) {
        body.concat(reinterpret_cast<const char *>(buffer), bytesRead);
    }

    return body.length() > 0;
}

static void handlePutConfig(HTTPRequest *req, HTTPResponse *res) {
    Serial.println("MeshText: PUT config hit");

    String body;
    if (!readRequestBody(req, body)) {
        res->setStatusCode(400);
        res->setHeader("Content-Type", "text/plain");
        res->print("Empty body");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        res->setStatusCode(400);
        res->setHeader("Content-Type", "text/plain");
        res->print("Bad JSON");
        return;
    }

    const char *name = doc["name"] | "";
    int category = doc["category"] | 0;

    // Save MeshText-specific category
    MeshTextConfig mtcfg;
    mtcfg.category = category;
    if (!saveMeshTextConfig(mtcfg)) {
        res->setStatusCode(500);
        res->setHeader("Content-Type", "text/plain");
        res->print("Failed to save MeshText config");
        return;
    }

    // Save Meshtastic node name
    strncpy(owner.short_name, name, sizeof(owner.short_name) - 1);
    owner.short_name[sizeof(owner.short_name) - 1] = '\0';

    // Optional: mirror into long_name too
    strncpy(owner.long_name, name, sizeof(owner.long_name) - 1);
    owner.long_name[sizeof(owner.long_name) - 1] = '\0';

    // Persist Meshtastic device state
    nodeDB->saveToDisk();

    JsonDocument outDoc;
    outDoc["ok"] = true;

    String out;
    serializeJson(outDoc, out);

    res->setStatusCode(200);
    res->setHeader("Content-Type", "application/json; charset=utf-8");
    res->print(out);
}

static void handlePutPage(HTTPRequest *req, HTTPResponse *res) {
    Serial.println("MeshText: PUT page hit");
    std::string path = req->getRequestString();

    std::size_t idx = path.find("/api/page/");
    if (idx == std::string::npos) {
        res->setStatusCode(400);
        res->setHeader("Content-Type", "text/plain");
        res->print("Bad request");
        return;
    }

    std::size_t start = idx + 10;
    std::size_t end = path.find(' ', start);
    std::string numStr = (end == std::string::npos)
        ? path.substr(start)
        : path.substr(start, end - start);

    int num = atoi(numStr.c_str());
    if (num < 1 || num > 255) {
        res->setStatusCode(400);
        res->setHeader("Content-Type", "text/plain");
        res->print("Invalid page");
        return;
    }

    String body;
    if (!readRequestBody(req, body)) {
        res->setStatusCode(400);
        res->setHeader("Content-Type", "text/plain");
        res->print("Empty body");
        return;
    }

    Serial.print("Body: ");
    Serial.println(body);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        res->setStatusCode(400);
        res->setHeader("Content-Type", "text/plain");
        res->print("Invalid JSON");
        return;
    }

    Page p{};
    p.page_num = static_cast<uint8_t>(num);
    p.flags = doc["flags"] | 0;

    const char *title = doc["title"] | "";
    strncpy(p.title, title, sizeof(p.title) - 1);
    p.title[sizeof(p.title) - 1] = '\0';

    JsonArray cells = doc["cells"].as<JsonArray>();
    for (int i = 0; i < PAGE_CELLS; i++) {
        p.cells[i] = 0;
    }

    if (!cells.isNull()) {
        int i = 0;
        for (JsonVariant v : cells) {
            if (i >= PAGE_CELLS) break;
            p.cells[i++] = v.as<uint8_t>();
        }
    }

    if (!savePage(static_cast<uint8_t>(num), p)) {
        res->setStatusCode(500);
        res->setHeader("Content-Type", "text/plain");
        res->print("Save failed");
        return;
    }

    JsonDocument outDoc;
    outDoc["ok"] = true;
    outDoc["page_num"] = p.page_num;

    String out;
    serializeJson(outDoc, out);

    res->setStatusCode(200);
    res->setHeader("Content-Type", "application/json; charset=utf-8");
    res->print(out);
}

void registerRoutes(HTTPServer *server) {
    ensurePagesDir();

    server->registerNode(new ResourceNode("/meshtext", "GET", &handleEditor));
    server->registerNode(new ResourceNode("/api/config", "GET", &handleConfig));
    server->registerNode(new ResourceNode("/api/pages", "GET", &handlePages));
    server->registerNode(new ResourceNode("/api/page/*", "GET", &handlePage));
    server->registerNode(new ResourceNode("/api/page/*", "PUT", &handlePutPage));
    server->registerNode(new ResourceNode("/api/config", "PUT", &handlePutConfig));
    
    Serial.println("MeshText: routes registered");

}

} // namespace meshtext