#ifndef __m3u8variant__h
#define __m3u8variant__h
#include <map>
#include <vector>
#include <string>
#include <sstream>

#ifndef TEST
#include <lib/base/wrappers.h>
#else
#include "wrappers.h"
#endif

#include "common.h"

typedef std::map<std::string,std::string> HeaderMap;

struct M3U8StreamInfo
{
    std::string url;
    std::string codecs;
    std::string resolution;
    // TODO audio/video/subtitles..
    unsigned long int bitrate;

    bool operator<(const M3U8StreamInfo& m) const
    {
        return bitrate < m.bitrate;
    }
};

class M3U8VariantsExplorer
{
    std::string url;
    HeaderMap headers;
    std::vector<M3U8StreamInfo> streams;
    const unsigned int redirectLimit;
    int parseStreamInfoAttributes(const char *line, M3U8StreamInfo& info);
    int getVariantsFromMasterUrl(const std::string& url, const HeaderMap& headers, unsigned int redirect);
public:
    M3U8VariantsExplorer(const std::string& url, const HeaderMap& headers):
        url(url),
        headers(headers),
        redirectLimit(3){};
    std::vector<M3U8StreamInfo> getStreams();

};

bool isM3U8Url(const std::string& url);
#endif

