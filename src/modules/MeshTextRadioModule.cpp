#include "MeshTextRadioModule.h"

#include "FSCommon.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "configuration.h"
#include "mesh/meshtext/MeshTextStorage.h"

MeshTextRadioModule *meshTextRadioModule = nullptr;

static constexpr meshtastic_PortNum MESHTEXT_PORT = static_cast<meshtastic_PortNum>(300);
//static constexpr meshtastic_PortNum MESHTEXT_PORT  = meshtastic_PortNum_TEXT_MESSAGE_APP;
static constexpr uint32_t ANNOUNCE_INTERVAL_MS = 60000;
static constexpr uint32_t BOOT_DELAY_MS = 5000;

#pragma pack(push, 1)

struct MeshTextAnnounce {
    uint8_t type;
    uint8_t version;
    uint8_t page_count;
    uint8_t first_page;
    uint8_t last_page;
    char name[16];
};

struct MeshTextRequest {
    uint8_t type;
    uint8_t version;
    uint32_t target_node;
    uint8_t page_num;
};

struct MeshTextResponse {
    uint8_t type;
    uint8_t version;
    uint32_t target_node;
    uint32_t source_node;
    uint8_t page_num;
    uint8_t flags;
    char title[16];
    uint8_t cells[meshtext::PAGE_CELLS];
};

#pragma pack(pop)

enum MeshTextPacketType : uint8_t {
    MESHTEXT_PKT_ANNOUNCE = 1,
    MESHTEXT_PKT_REQUEST  = 2,
    MESHTEXT_PKT_RESPONSE = 3,
};

MeshTextRadioModule::MeshTextRadioModule()
    : SinglePortModule("meshtext", MESHTEXT_PORT),
      OSThread("MeshTextRadio")
{
    loopbackOk = true;
    Serial.println("MeshTextRadioModule ctor");
}

int32_t MeshTextRadioModule::runOnce()
{
    if (firstTime) {
        firstTime = false;
        lastAnnounceMs = millis() - (ANNOUNCE_INTERVAL_MS - BOOT_DELAY_MS);
        return BOOT_DELAY_MS;
    }

    if ((millis() - lastAnnounceMs) >= ANNOUNCE_INTERVAL_MS) {
        sendAnnounce();
        lastAnnounceMs = millis();
    }

    return 1000;
}

bool MeshTextRadioModule::sendAnnounce()
{
    if (channels.isDefaultChannel(CHANNEL_INDEX)) {
        LOG_WARN("MeshText: channel %d is default/public, not sending", CHANNEL_INDEX);
        return false;
    }

    meshtext::PageListEntry list[meshtext::MAX_PAGES];
    uint8_t count = meshtext::listPages(list, meshtext::MAX_PAGES);

    MeshTextAnnounce ann{};
    ann.type = MESHTEXT_PKT_ANNOUNCE;
    ann.version = 1;
    ann.page_count = count;
    ann.first_page = count ? list[0].page_num : 0;
    ann.last_page  = count ? list[count - 1].page_num : 0;

    strncpy(ann.name, owner.short_name, sizeof(ann.name) - 1);
    ann.name[sizeof(ann.name) - 1] = '\0';

    meshtastic_MeshPacket *p = allocDataPacket();
    p->want_ack = false;
    p->channel = CHANNEL_INDEX;
    p->to = NODENUM_BROADCAST;

    p->decoded.payload.size = sizeof(ann);
    memcpy(p->decoded.payload.bytes, &ann, sizeof(ann));

    LOG_INFO("MeshText: announce on channel %d, pages=%u", CHANNEL_INDEX, count);
    service->sendToMesh(p);
    return true;
}

bool MeshTextRadioModule::requestPage(uint32_t toNode, uint8_t pageNum)
{
    MeshTextRequest req{};
    req.type = MESHTEXT_PKT_REQUEST;
    req.version = 1;
    req.target_node = toNode;
    req.page_num = pageNum;

    meshtastic_MeshPacket *p = allocDataPacket();
    p->want_ack = false;
    p->channel = CHANNEL_INDEX;
    p->to = NODENUM_BROADCAST;
    p->decoded.payload.size = sizeof(req);
    memcpy(p->decoded.payload.bytes, &req, sizeof(req));

    Serial.printf("MeshText TX request: to=0x%x page=%u\n", toNode, pageNum);
    service->sendToMesh(p);
    return true;
}

bool MeshTextRadioModule::sendPageResponse(uint32_t toNode, const meshtext::Page &page)
{
    MeshTextResponse resp{};
    resp.type = MESHTEXT_PKT_RESPONSE;
    resp.version = 1;
    resp.target_node = toNode;
    resp.source_node = nodeDB->getNodeNum();
    resp.page_num = page.page_num;
    resp.flags = page.flags;
    memcpy(resp.title, page.title, sizeof(resp.title));
    memcpy(resp.cells, page.cells, sizeof(resp.cells));

    meshtastic_MeshPacket *p = allocDataPacket();
    p->want_ack = false;
    p->channel = CHANNEL_INDEX;
    p->to = NODENUM_BROADCAST;
    p->decoded.payload.size = sizeof(resp);
    memcpy(p->decoded.payload.bytes, &resp, sizeof(resp));

    Serial.printf("MeshText TX response: to=0x%x page=%u\n", toNode, page.page_num);
    service->sendToMesh(p);
    return true;
}

ProcessMessage MeshTextRadioModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    Serial.printf("MeshText handleReceived raw: from=0x%x ch=%d port=%d size=%d\n",
                  mp.from, mp.channel, mp.decoded.portnum, mp.decoded.payload.size);

    if (mp.channel != CHANNEL_INDEX) {
        return ProcessMessage::CONTINUE;
    }

    if (mp.from == nodeDB->getNodeNum()) {
        return ProcessMessage::CONTINUE;
    }

    if (mp.decoded.payload.size < 1) {
        return ProcessMessage::CONTINUE;
    }

    const uint8_t *buf = mp.decoded.payload.bytes;
    uint8_t type = buf[0];

    if (type == MESHTEXT_PKT_ANNOUNCE &&
        mp.decoded.payload.size == sizeof(MeshTextAnnounce)) {

        MeshTextAnnounce ann{};
        memcpy(&ann, buf, sizeof(ann));

        meshtext::upsertRemoteSource(
            mp.from,
            ann.name,
            ann.page_count,
            ann.first_page,
            ann.last_page,
            mp.channel);

        Serial.printf("MeshText RX announce: from=0x%x name=%s pages=%u range=%u-%u\n",
                      mp.from, ann.name, ann.page_count, ann.first_page, ann.last_page);

        return ProcessMessage::CONTINUE;
    }

    if (type == MESHTEXT_PKT_REQUEST &&
        mp.decoded.payload.size == sizeof(MeshTextRequest)) {

        MeshTextRequest req{};
        memcpy(&req, buf, sizeof(req));

        if(req.target_node != nodeDB->getNodeNum()){
            return ProcessMessage::CONTINUE;
        }

        Serial.printf("MeshText RX request: from=0x%x page=%u\n", mp.from, req.page_num);

        meshtext::Page page{};
        if (meshtext::loadPage(req.page_num, page)) {
            sendPageResponse(mp.from, page);
        }

        return ProcessMessage::CONTINUE;
    }

    if (type == MESHTEXT_PKT_RESPONSE &&
        mp.decoded.payload.size == sizeof(MeshTextResponse)) {

        MeshTextResponse resp{};
        memcpy(&resp, buf, sizeof(resp));

        if(resp.target_node != nodeDB->getNodeNum()){
            return ProcessMessage::CONTINUE;
        }
        
        meshtext::Page page{};
        page.page_num = resp.page_num;
        page.flags = resp.flags;
        memcpy(page.title, resp.title, sizeof(page.title));
        memcpy(page.cells, resp.cells, sizeof(page.cells));

        meshtext::storeLatestRemotePage(resp.source_node, page);

        Serial.printf("MeshText RX response: from=0x%x page=%u title=%s\n",
                      resp.source_node, page.page_num, page.title);

        return ProcessMessage::CONTINUE;
    }

    return ProcessMessage::CONTINUE;
}