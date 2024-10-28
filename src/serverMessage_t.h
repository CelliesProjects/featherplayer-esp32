#ifndef _SERVER_MESSGE_
#define _SERVER_MESSGE_
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
    size_t value;
    size_t value2;
    char str[160]{};
    bool singleClient = false;
};

#endif