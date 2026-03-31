#pragma once

#include "SinglePortModule.h"

class MeshTextRadioModule : public SinglePortModule, private concurrency::OSThread {
public:
    MeshTextRadioModule();

    static constexpr uint8_t CHANNEL_INDEX = 1;

protected:
    virtual int32_t runOnce() override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

private:
    bool firstTime = true;
    uint32_t lastAnnounceMs = 0;

    bool sendAnnounce();
};

extern MeshTextRadioModule *meshTextRadioModule;