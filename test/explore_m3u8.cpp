#include <cstdlib>
#include <iostream>
#include <vector>
#include <map>
#include "m3u8.h"


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
            printf("%15s: ", "headers");
            int j = 0;
            for (HeaderMap::const_iterator it(iter->headers.begin()); it != iter->headers.end(); it++, j++)
            {
                if (j != 0)
                    printf(", ");
                printf("\"%s: %s\"", it->first.c_str(), it->second.c_str());
            }
            printf("\n");
            printf("%15s: %ld\n", "bitrate", iter->bitrate);
            printf("%15s: %s\n", "resolution", iter->resolution.c_str());
            printf("%15s: %s\n", "codecs", iter->codecs.c_str());
        }
    }
    return 0;
}
