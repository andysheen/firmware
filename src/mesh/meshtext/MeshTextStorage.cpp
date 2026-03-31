#include "MeshTextStorage.h"
#include "FSCommon.h"
#include "NodeDB.h"

namespace meshtext {

static uint8_t currentPageNum = 0;
static RemotePageCache latestRemotePage = {};
static RemotePageSource remoteSources[MAX_REMOTE_SOURCES] = {};
static bool welcomeChecked = false;

void setCurrentPage(uint8_t pageNum) {
    currentPageNum = pageNum;
}

uint8_t getCurrentPage() {
    return currentPageNum;
}

static void pagePath(uint8_t num, char *buf, size_t len) {
    snprintf(buf, len, "/pages/%03u.bin", num);
}

static void writeTextToPage(meshtext::Page &page, const char *text)
{
    for (int i = 0; i < meshtext::PAGE_CELLS; i++) {
        page.cells[i] = ' ';
    }

    int idx = 0;
    for (const char *p = text; *p && idx < meshtext::PAGE_CELLS; ++p) {
        if (*p == '\n') {
            int row = idx / meshtext::PAGE_COLS;
            idx = (row + 1) * meshtext::PAGE_COLS;
            continue;
        }
        page.cells[idx++] = static_cast<uint8_t>(*p);
    }
}

bool ensureDefaultWelcomePage()
{
    if (welcomeChecked) {
        return true;
    }
    welcomeChecked = true;

    // IMPORTANT: do NOT call listPages() here, because that calls ensurePagesDir()
    File dir = FSCom.open("/pages");
    if (!dir || !dir.isDirectory()) {
        return false;
    }

    bool hasAnyPage = false;
    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory() && f.size() == sizeof(Page)) {
            hasAnyPage = true;
            f.close();
            break;
        }
        f.close();
        f = dir.openNextFile();
    }
    dir.close();

    if (hasAnyPage) {
        return true;
    }

    Page page{};
    page.page_num = 100;
    page.flags = 0;
    strncpy(page.title, "Welcome", sizeof(page.title) - 1);
    page.title[sizeof(page.title) - 1] = '\0';

    char body[160];
    snprintf(body, sizeof(body),
             "Welcome to MeshText\n"
             "my name is %s\n"
             "\n"
             "Use /meshtext\n"
             "to edit pages",
             owner.short_name[0] ? owner.short_name : "Meshtastic");

    writeTextToPage(page, body);

    char path[24];
    pagePath(page.page_num, path, sizeof(path));

    File out = FSCom.open(path, "w");
    if (!out) {
        return false;
    }

    size_t n = out.write(reinterpret_cast<const uint8_t *>(&page), sizeof(Page));
    out.close();

    if (n != sizeof(Page)) {
        return false;
    }

    setCurrentPage(page.page_num);
    return true;
}

bool ensurePagesDir()
{
    if (!FSCom.exists("/pages")) {
        if (!FSCom.mkdir("/pages")) {
            return false;
        }
    }

    return ensureDefaultWelcomePage();
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