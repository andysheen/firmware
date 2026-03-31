#include "MeshTextRenderer.h"
#include "MeshTextStorage.h"


namespace meshtext {

// 20 x 8 cells on a 128 x 64 OLED.
// Upstream renderer uses a 6x8-ish cell model and leaves a small x offset. :contentReference[oaicite:2]{index=2}

static constexpr int CELL_W = 6;
static constexpr int CELL_H = 8;
static constexpr int GRID_X_OFFSET = 4;

// Block graphics are 2 columns x 3 rows of sub-rectangles inside a cell.
// Upstream maps bits 0-5 to that 2x3 grid. :contentReference[oaicite:3]{index=3}
static constexpr int BLOCK_SUB_W = 3;
static constexpr int BLOCK_SUB_H = 2;

// Control cell values based on the upstream renderer logic.
// The renderer checks explicit symbolic cells for link/dynamic types, but the
// exact numeric constants live elsewhere in that repo. The values below are
// placeholders until you copy the original constants from MeshTEXT headers.
// Replace these with the real ones from the MeshTEXT type/constant header.
static constexpr uint8_t CELL_LINK           = 0x01;
static constexpr uint8_t CELL_VISITOR_COUNT  = 0x02;
static constexpr uint8_t CELL_VOTE_A_BTN     = 0x03;
static constexpr uint8_t CELL_VOTE_B_BTN     = 0x04;
static constexpr uint8_t CELL_VOTE_A_COUNT   = 0x05;
static constexpr uint8_t CELL_VOTE_B_COUNT   = 0x06;

static inline bool isDynamicCell(uint8_t cell) {
    return cell == CELL_VISITOR_COUNT ||
           cell == CELL_VOTE_A_BTN ||
           cell == CELL_VOTE_B_BTN ||
           cell == CELL_VOTE_A_COUNT ||
           cell == CELL_VOTE_B_COUNT;
}

static void drawGlyph(OLEDDisplay *display, int16_t x, int16_t y, char c) {
    char buf[2] = {c, '\0'};
    display->drawString(x, y, String(buf));
}

static void drawBlockCell(OLEDDisplay *display, int16_t x, int16_t y, uint8_t cell) {
    for (uint8_t bit = 0; bit < 6; bit++) {
        if (cell & (1 << bit)) {
            int16_t col = bit & 1;
            int16_t row = bit >> 1;
            int16_t rx = x + col * BLOCK_SUB_W;
            int16_t ry = y + row * BLOCK_SUB_H;
            display->fillRect(rx, ry, BLOCK_SUB_W, BLOCK_SUB_H);
        }
    }
}

static uint8_t drawNumber(OLEDDisplay *display, int16_t x, int16_t y, uint16_t num) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%u", num);
    uint8_t len = strlen(buf);
    for (uint8_t i = 0; i < len; i++) {
        drawGlyph(display, x + i * CELL_W, y, buf[i]);
    }
    return len;
}

static void drawButton(OLEDDisplay *display, int16_t x, int16_t y, char label, bool selected) {
    if (selected) {
        display->fillRect(x, y, CELL_W, CELL_H);
        // Fake invert: clear inside and redraw border-ish look.
        // SSD1306 display lib does not expose the same draw color flipping as U8G2.
        display->setColor(BLACK);
        drawGlyph(display, x, y, label);
        display->setColor(WHITE);
    } else {
        drawGlyph(display, x, y, label);
        display->drawHorizontalLine(x, y + CELL_H - 1, CELL_W);
    }
}

static uint8_t drawLink(OLEDDisplay *display, int16_t x, int16_t y, uint8_t targetPage, bool selected) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%u", targetPage);
    uint8_t numLen = strlen(buf);
    uint8_t totalCells = 1 + numLen;

    if (selected) {
        display->fillRect(x, y, totalCells * CELL_W, CELL_H);
        display->setColor(BLACK);
        drawGlyph(display, x, y, '>');
        for (uint8_t i = 0; i < numLen; i++) {
            drawGlyph(display, x + (1 + i) * CELL_W, y, buf[i]);
        }
        display->setColor(WHITE);
    } else {
        drawGlyph(display, x, y, '>');
        for (uint8_t i = 0; i < numLen; i++) {
            drawGlyph(display, x + (1 + i) * CELL_W, y, buf[i]);
        }
        display->drawHorizontalLine(x, y + CELL_H - 1, totalCells * CELL_W);
    }

    return totalCells;
}

void drawPage(OLEDDisplay *display, const Page &page, const ReactTally *tally, int16_t selectedCellIdx) {
    bool skip[PAGE_CELLS] = {};

    // Upstream first marks spill-over cells for dynamic numeric fields. :contentReference[oaicite:4]{index=4}
    if (tally) {
        for (uint8_t i = 0; i < PAGE_CELLS; i++) {
            uint8_t cell = page.cells[i];
            uint16_t val = 0;

            if (cell == CELL_VISITOR_COUNT) val = tally->visits;
            else if (cell == CELL_VOTE_A_COUNT) val = tally->votes_a;
            else if (cell == CELL_VOTE_B_COUNT) val = tally->votes_b;
            else continue;

            char buf[6];
            uint8_t len = snprintf(buf, sizeof(buf), "%u", val);
            uint8_t row = i / PAGE_COLS;
            for (uint8_t d = 1; d < len && (i + d) / PAGE_COLS == row; d++) {
                skip[i + d] = true;
            }
        }
    }

    // Upstream link rendering uses one cell for the link marker and the next
    // cell as the target page number, then skips any extra cells used by the
    // rendered digits. :contentReference[oaicite:5]{index=5}
    for (uint8_t i = 0; i < PAGE_CELLS; i++) {
        if (page.cells[i] == CELL_LINK && i + 1 < PAGE_CELLS) {
            skip[i + 1] = true;

            uint8_t target = page.cells[i + 1];
            char buf[6];
            uint8_t numLen = snprintf(buf, sizeof(buf), "%u", target);
            uint8_t row = i / PAGE_COLS;

            for (uint8_t d = 2; d < 1 + numLen && (i + d) / PAGE_COLS == row; d++) {
                skip[i + d] = true;
            }
        }
    }

    // Meshtastic already clears per frame, but doing it here makes this helper standalone.
    // Callers can remove this if they prefer.
    // display->clear();

    for (uint8_t row = 0; row < PAGE_ROWS; row++) {
        for (uint8_t col = 0; col < PAGE_COLS; col++) {
            uint8_t idx = row * PAGE_COLS + col;
            if (skip[idx]) continue;

            uint8_t cell = page.cells[idx];
            int16_t x = GRID_X_OFFSET + col * CELL_W;
            int16_t y = row * CELL_H;

            if (cell == CELL_LINK && idx + 1 < PAGE_CELLS) {
                uint8_t target = page.cells[idx + 1];
                bool selected = (selectedCellIdx == (int16_t)idx);
                drawLink(display, x, y, target, selected);
            } else if (isDynamicCell(cell) && tally) {
                switch (cell) {
                    case CELL_VISITOR_COUNT:
                        drawNumber(display, x, y, tally->visits);
                        break;
                    case CELL_VOTE_A_BTN:
                        drawButton(display, x, y, 'A', selectedCellIdx == (int16_t)idx);
                        break;
                    case CELL_VOTE_B_BTN:
                        drawButton(display, x, y, 'B', selectedCellIdx == (int16_t)idx);
                        break;
                    case CELL_VOTE_A_COUNT:
                        drawNumber(display, x, y, tally->votes_a);
                        break;
                    case CELL_VOTE_B_COUNT:
                        drawNumber(display, x, y, tally->votes_b);
                        break;
                }
            } else if (cell >= 0x80 && cell <= 0xBF) {
                drawBlockCell(display, x, y, cell & 0x3F);
            } else if (cell >= 0x20 && cell <= 0x7E) {
                drawGlyph(display, x, y, (char)cell);
            }
        }
    }
}

} // namespace meshtext