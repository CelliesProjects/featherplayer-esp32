#include "playList.h"

String playList_t::toString()
{
    String s = "playlist\n";
    s.concat(currentItem());
    s.concat('\n');

    std::lock_guard<std::mutex> lock(listMutex);

    for (const auto &item : list)
    {
        if (item.type < 0 || item.type >= sizeof(typeStr) / sizeof(typeStr[0]))
        {
            log_e("ERROR! Playlist item has an invalid type: %d", item.type);
            continue;
        }

        switch (item.type)
        {
        case HTTP_FILE:
            s.concat(item.url.substring(item.url.lastIndexOf("/") + 1) + "\n" + typeStr[item.type] + "\n");
            break;

        case HTTP_PRESET:
            if (item.index < NUMBER_OF_PRESETS)
                s.concat(preset[item.index].name + "\n" + typeStr[item.type] + "\n");
            else
                log_e("ERROR! Invalid preset index: %u", item.index);
            break;

        case HTTP_FOUND:
        case HTTP_FAVORITE:
            s.concat(item.name + "\n" + typeStr[item.type] + "\n");
            break;

        default:
            log_e("ERROR! Unhandled item type: %d", item.type);
            break;
        }
    }
    return s;
}
