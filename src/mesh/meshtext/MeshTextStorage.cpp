#include "MeshTextStorage.h"
#include "FSCommon.h"

namespace meshtext {

static uint8_t currentPageNum = 0;
static RemotePageCache latestRemotePage = {};

void setCurrentPage(uint8_t pageNum) {
    currentPageNum = pageNum;
}

uint8_t getCurrentPage() {
    return currentPageNum;
}

uint8_t listPages(PageListEntry *list, uint8_t maxEntries)
{
    if (!ensurePagesDir()) return 0;

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

bool ensurePagesDir() {
    if (!FSCom.exists("/pages")) {
        return FSCom.mkdir("/pages");
    }
    return true;
}

static void pagePath(uint8_t num, char *buf, size_t len) {
    snprintf(buf, len, "/pages/%03u.bin", num);
}

bool loadPage(uint8_t num, Page &page) {
    char path[24];
    pagePath(num, path, sizeof(path));

    File f = FSCom.open(path, "r");
    if (!f) return false;
    if (f.size() != sizeof(Page)) {
        f.close();
        return false;
    }

    size_t n = f.read(reinterpret_cast<uint8_t *>(&page), sizeof(Page));
    f.close();
    return n == sizeof(Page);
}

bool savePage(uint8_t num, const Page &page) {
    char path[24];
    pagePath(num, path, sizeof(path));

    File f = FSCom.open(path, "w");
    if (!f) return false;

    size_t n = f.write(reinterpret_cast<const uint8_t *>(&page), sizeof(Page));
    f.close();
    return n == sizeof(Page);
}

bool getFirstPageNumber(uint8_t &pageNum) {
    if (!ensurePagesDir()) return false;

    File dir = FSCom.open("/pages");
    if (!dir || !dir.isDirectory()) return false;

    bool found = false;
    uint8_t lowest = 255;

    File f = dir.openNextFile();
    while (f) {
        if (f.size() == sizeof(Page)) {
            Page p{};
            if (f.read(reinterpret_cast<uint8_t *>(&p), sizeof(Page)) == sizeof(Page)) {
                if (!found || p.page_num < lowest) {
                    lowest = p.page_num;
                    found = true;
                }
            }
        }
        f.close();
        f = dir.openNextFile();
    }

    dir.close();

    if (found) {
        pageNum = lowest;
        return true;
    }
    return false;
}

bool loadFirstPage(Page &page)
{
    uint8_t first = 0;
    if (!getFirstPageNumber(first)) return false;
    if (!loadPage(first, page)) return false;
    setCurrentPage(first);
    return true;
}

static RemotePageSource remoteSources[MAX_REMOTE_SOURCES] = {};

void upsertRemoteSource(uint32_t fromNode,
                                  const char *name,
                                  uint8_t pageCount,
                                  uint8_t firstPage,
                                  uint8_t lastPage,
                                  int8_t channelIndex)
{
    int freeIdx = -1;
    int foundIdx = -1;

    for (int i = 0; i < MAX_REMOTE_SOURCES; ++i) {
        if (remoteSources[i].active && remoteSources[i].fromNode == fromNode) {
            foundIdx = i;
            break;
        }
        if (!remoteSources[i].active && freeIdx < 0) {
            freeIdx = i;
        }
    }

    int idx = (foundIdx >= 0) ? foundIdx : freeIdx;
    if (idx < 0) return;

    auto &r = remoteSources[idx];
    r.active = true;
    r.fromNode = fromNode;
    strncpy(r.name, name ? name : "", sizeof(r.name) - 1);
    r.name[sizeof(r.name) - 1] = '\0';
    r.page_count = pageCount;
    r.first_page = firstPage;
    r.last_page = lastPage;
    r.channelIndex = channelIndex;
    r.lastSeenMs = millis();
}

uint8_t getRemoteSources(RemotePageSource *out, uint8_t maxEntries)
{
    uint8_t count = 0;
    for (int i = 0; i < MAX_REMOTE_SOURCES && count < maxEntries; ++i) {
        if (remoteSources[i].active) {
            out[count++] = remoteSources[i];
        }
    }
    return count;
}

void storeLatestRemotePage(uint32_t fromNode, const Page &page)
{
    latestRemotePage.valid = true;
    latestRemotePage.fromNode = fromNode;
    latestRemotePage.page = page;
    latestRemotePage.receivedAtMs = millis();
}

bool getLatestRemotePage(RemotePageCache &out)
{
    if (!latestRemotePage.valid) return false;
    out = latestRemotePage;
    return true;
}

void clearLatestRemotePage()
{
    latestRemotePage.valid = false;
}


}