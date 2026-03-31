#include "MeshTextStorage.h"
#include "FSCommon.h"

namespace meshtext {

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

bool loadFirstPage(Page &page) {
    uint8_t first = 0;
    if (!getFirstPageNumber(first)) return false;
    return loadPage(first, page);
}

}