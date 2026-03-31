#pragma once

#include <OLEDDisplay.h>
#include "MeshTextStorage.h"

namespace meshtext {

struct ReactTally {
    uint16_t visits = 0;
    uint16_t votes_a = 0;
    uint16_t votes_b = 0;
};

// Draw one MeshText page onto the Meshtastic OLED display.
void drawPage(OLEDDisplay *display, const Page &page, const ReactTally *tally = nullptr, int16_t selectedCellIdx = -1);

}