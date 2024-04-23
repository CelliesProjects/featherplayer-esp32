#ifndef PLAYLIST_FEATHER
#define PLAYLIST_FEATHER

#include <Arduino.h>
#include <vector>
#include <mutex>
#include "presets.h"

#define PLAYLIST_MAX_ITEMS 100
#define PLAYLIST_MAX_URL_LENGTH 255
#define PLAYLIST_STOPPED -1 /* do not change */

enum streamType
{
    HTTP_FILE,
    HTTP_FOUND,
    HTTP_FAVORITE,
    HTTP_PRESET,
    TYPE_ERROR
};

[[maybe_unused]] static const char *typeStr[] = {"FILE", "FOUND", "FAVO", "PRESET", "ERROR"};

struct playListItem
{
    streamType type;
    String name;
    String url;
    uint32_t index; /* the presets index*/
};

class playList_t
{

public:
    playList_t() : listMutex(), currentItemMutex() {}

    ~playList_t()
    {
        clear();
    }

    int size()
    {
        std::lock_guard<std::mutex> lock(listMutex);
        return list.size();
    }

    void get(const uint32_t index, playListItem &item)
    {
        std::lock_guard<std::mutex> lock(listMutex);
        item = (index < list.size()) ? list[index] : (playListItem){};
    }

    const String url(const uint32_t index)
    {
        std::lock_guard<std::mutex> lock(listMutex);
        return (index < list.size()) ? ((list[index].type == HTTP_PRESET) ? preset[list[index].index].url : list[index].url) : "";
    }

    streamType type(const uint32_t index)
    {
        std::lock_guard<std::mutex> lock(listMutex);
        if (list.size() < index)
            return TYPE_ERROR;
        return list[index].type;
    }

    const String name(const uint32_t index)
    {
        std::lock_guard<std::mutex> lock(listMutex);
        if (index >= list.size())
            return "";
        switch (list[index].type)
        {
        case HTTP_FILE:
            return list[index].url.substring(list[index].url.lastIndexOf('/') + 1);
        case HTTP_PRESET:
            return preset[list[index].index].name;
        default:
            return list[index].name;
        }
    }

    void add(const playListItem &item)
    {
        std::lock_guard<std::mutex> lock(listMutex);
        if (list.size() < PLAYLIST_MAX_ITEMS)
            list.push_back(item);
    }

    void remove(const uint32_t index)
    {
        std::lock_guard<std::mutex> lock(listMutex);
        if (list.size() > index)
            list.erase(list.begin() + index);
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(listMutex);
        list.clear();
    }

    const String &toString(String &s);

    int8_t currentItem()
    {
        std::lock_guard<std::mutex> lock(currentItemMutex);
        return _currentItem;
    }

    void setCurrentItem(int8_t index)
    {
        std::lock_guard<std::mutex> lock(currentItemMutex);
        _currentItem = index;
    }

private:
    std::vector<playListItem> list;
    std::mutex listMutex;
    int8_t _currentItem{PLAYLIST_STOPPED};
    std::mutex currentItemMutex;
};

#endif
