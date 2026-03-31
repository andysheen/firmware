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
    uint8_t version;
    uint8_t page_count;
    uint8_t first_page;
    uint8_t last_page;
    uint8_t reserved;
    char name[16];
};
#pragma pack(pop)

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

ProcessMessage MeshTextRadioModule::handleReceived(const meshtastic_MeshPacket &mp)
{

      Serial.printf("MeshText handleReceived raw: from=0x%x ch=%d port=%d size=%d\n",
              mp.from, mp.channel, mp.decoded.portnum, mp.decoded.payload.size);
              
    if (mp.channel != CHANNEL_INDEX) {
        return ProcessMessage::CONTINUE;
    }

    if (mp.decoded.payload.size < sizeof(MeshTextAnnounce)) {
        return ProcessMessage::CONTINUE;
    }

    MeshTextAnnounce ann{};
    memcpy(&ann, mp.decoded.payload.bytes, sizeof(ann));
  
    meshtext::upsertRemoteSource(
        mp.from,
        ann.name,
        ann.page_count,
        ann.first_page,
        ann.last_page,
        mp.channel);

    LOG_INFO("MeshText: RX announce from=0x%x name=%s pages=%u range=%u-%u",
             mp.from, ann.name, ann.page_count, ann.first_page, ann.last_page);

    return ProcessMessage::CONTINUE;
}