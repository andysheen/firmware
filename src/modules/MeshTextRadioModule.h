#pragma once

#include "SinglePortModule.h"
#include "mesh/meshtext/MeshTextStorage.h"

class MeshTextRadioModule : public SinglePortModule, private concurrency::OSThread {
public:
    MeshTextRadioModule();

    static constexpr uint8_t CHANNEL_INDEX = 1;
    bool requestPage(uint32_t toNode, uint8_t pageNum);
protected:
    virtual int32_t runOnce() override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

private:
    bool firstTime = true;
    uint32_t lastAnnounceMs = 0;

    bool sendPageResponse(uint32_t toNode, const meshtext::Page &page);
    bool sendAnnounce();
};

extern MeshTextRadioModule *meshTextRadioModule;