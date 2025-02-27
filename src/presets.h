#ifndef __PRESETS__
#define __PRESETS__

struct source
{
    const String name;
    const String url;
};

const struct source preset[]{
    {"NPO Radio1", "http://icecast.omroep.nl/radio1-bb-mp3"},
    {"NPO Radio2", "http://icecast.omroep.nl/radio2-bb-mp3"},
    {"NPO Soul&Jazz", "http://icecast.omroep.nl/radio6-bb-mp3"},
    {"NPO 3FM", "http://icecast.omroep.nl/3fm-bb-mp3"},
    {"NPO Klassiek", "http://icecast.omroep.nl/radio4-bb-mp3"},
    {"NPO Radio5", "http://icecast.omroep.nl/radio5-bb-mp3"},
    {"Absoluut FM", "http://absoluutfm.stream.laut.fm/absoluutfm"},
    {"Amsterdam Funk Channel", "http://stream.afc.fm:8504/stream"},
    {"Radio Gelderland", "http://d2od87akyl46nm.cloudfront.net/icecast/omroepgelderland/radiogelderland"},
    {"Sublime Soul", "http://22673.live.streamtheworld.com/WEB22_MP3_SC?dist=SUBLIME_WEBSITE"},
    {"Sublime Jazz", "http://22673.live.streamtheworld.com/WEB20_MP3_SC?dist=SUBLIME_WEBSITE"},
    {"XXL Stenders", "http://streams.robstenders.nl:8063/bonanza_mp3"},
    {"LXClassics", "http://www.lxclassics.com/playlist/LXClassics.m3u"},
    {"192 Radio Nederland", "http://192radio.stream-server.nl/stream"},
    {"DELTA RADIO NIJMEGEN", "http://streamdelta.lokaalradio.nl:9005/download.mp3"},
    {"80s Hitradio Amsterdam", "http://s22.myradiostream.com:7728/"},
    {"538 Dance Department", "http://playerservices.streamtheworld.com/api/livestream-redirect/TLPSTR01.mp3"},
    {"Jazz Radio Funk", "http://jazz-wr06.ice.infomaniak.ch/jazz-wr06-128.mp3"},
    {"RadioEins", "http://radioeins.de/stream"},
    {"Tekno1", "https://tekno1.radioca.st/listen.pls?sid=1"},
    {"BollyHits Radio", "http://hoth.alonhosting.com:1080/stream/1/"},
};

constexpr const uint8_t NUMBER_OF_PRESETS = sizeof(preset) / sizeof(source);

#endif
