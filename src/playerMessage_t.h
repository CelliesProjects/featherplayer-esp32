#ifndef _PLAYER_MESSAGE_
#define _PLAYER_MESSAGE_

struct playerMessage
{
    enum Type
    {
        SET_VOLUME,
        START_ITEM,
        STOPSONG,
        PAUSE,
        RESUME,
    };
    Type type;
    bool singleClient = false;
    size_t value = 0;
    size_t offset = 0;
};

#endif
