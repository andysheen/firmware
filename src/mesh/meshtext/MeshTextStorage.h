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

struct PageListEntry {
    uint8_t page_num;
    char title[16];
};

struct RemotePageSource {
    uint32_t fromNode;
    char name[16];
    uint8_t page_count;
    uint8_t first_page;
    uint8_t last_page;
    int8_t channelIndex;
    uint32_t lastSeenMs;
    bool active;
};

struct RemotePageCache {
    bool valid;
    uint32_t fromNode;
    Page page;
    uint32_t receivedAtMs;
};

inline constexpr int MAX_REMOTE_SOURCES = 16;

void upsertRemoteSource(uint32_t fromNode,
                        const char *name,
                        uint8_t pageCount,
                        uint8_t firstPage,
                        uint8_t lastPage,
                        int8_t channelIndex);

uint8_t getRemoteSources(RemotePageSource *out, uint8_t maxEntries);
void storeLatestRemotePage(uint32_t fromNode, const Page &page);
bool getLatestRemotePage(RemotePageCache &out);
void clearLatestRemotePage();
bool ensurePagesDir();
bool loadPage(uint8_t num, Page &page);
bool savePage(uint8_t num, const Page &page);
bool getFirstPageNumber(uint8_t &pageNum);
bool loadFirstPage(Page &page);
uint8_t listPages(PageListEntry *list, uint8_t maxEntries);

void setCurrentPage(uint8_t pageNum);
uint8_t getCurrentPage();

bool ensureDefaultWelcomePage();

}