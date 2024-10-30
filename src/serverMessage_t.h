#ifndef _SERVER_MESSAGE_
#define _SERVER_MESSAGE_
struct serverMessage
{
    enum Type
    {
        WS_UPDATE_STATION,
        WS_UPDATE_STREAMTITLE,
        WS_UPDATE_NOWPLAYING,
        WS_UPDATE_FAVORITES,
        WS_UPDATE_PLAYLIST,
        WS_UPDATE_VOLUME,
        WS_PASS_MESSAGE,
        WS_UPDATE_PROGRESS,
        WS_UPDATE_STATUS
    };
    Type type;
    char str[160]{};
    bool singleClient = false;
    size_t value;
    size_t value2;
};

#endif
