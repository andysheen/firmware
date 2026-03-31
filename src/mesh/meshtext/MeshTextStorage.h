#pragma once

#include <Arduino.h>

namespace meshtext {

inline constexpr int PAGE_COLS = 20;
inline constexpr int PAGE_ROWS = 8;
inline constexpr int PAGE_CELLS = PAGE_COLS * PAGE_ROWS;
inline constexpr int MAX_PAGES = 32;

struct Page {
    uint8_t cells[PAGE_CELLS];
    uint8_t page_num;
    uint8_t flags;
    char title[16];
};

bool ensurePagesDir();
bool loadPage(uint8_t num, Page &page);
bool savePage(uint8_t num, const Page &page);
bool getFirstPageNumber(uint8_t &pageNum);
bool loadFirstPage(Page &page);

}