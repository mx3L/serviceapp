#include <cstdlib>
#include <iostream>
#include <vector>
#include <map>
#include "m3u8.h"

HeaderMap getHeaders(const std::string& url)
{
    HeaderMap headers;
    size_t pos = url.find('#');
    if (pos != std::string::npos && (url.compare(0, 4, "http") == 0 || url.compare(0, 4, "rtsp") == 0))
    {
        std::string headers_str = url.substr(pos + 1);
        pos = 0;
        while (pos != std::string::npos)
        {
            std::string name, value;
            size_t start = pos;
            size_t len = std::string::npos;
            pos = headers_str.find('=', pos);
            if (pos != std::string::npos)
            {
                len = pos - start;
                pos++;
                name = headers_str.substr(start, len);
                start = pos;
                len = std::string::npos;
                pos = headers_str.find('&', pos);
                if (pos != std::string::npos)
                {
                    len = pos - start;
                    pos++;
                }
                value = headers_str.substr(start, len);
            }
            if (!name.empty() && !value.empty())
            {
                headers[name] = value;
            }
        }
    }
    return headers;
}


int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s [url...]\n", argv[0]);
    }
    SSL_load_error_strings();
    SSL_library_init();
    while (--argc > 0)
    {
        std::string url = *(++argv);
        if (!isM3U8Url(url))
        {
            fprintf(stderr, "'%s' is not a valid m3u8 url!\n", url.c_str());
            continue;
        }
        M3U8VariantsExplorer ve(url, getHeaders(url));
        std::vector<M3U8StreamInfo> streams = ve.getStreams();
        int i = 0;
        for (std::vector<M3U8StreamInfo>::const_iterator iter(streams.begin()); iter != streams.end(); iter++, i++)
        {
            printf("HLS[%d]: %s\n", i, iter->url.c_str());
            printf("%15s: %ld\n", "bitrate", iter->bitrate);
            printf("%15s: %s\n", "resolution", iter->resolution.c_str());
            printf("%15s: %s\n", "codecs", iter->codecs.c_str());
        }
    }
    return 0;
}
