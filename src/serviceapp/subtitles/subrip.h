#ifndef __subripparser
#define __subripparser
#include <sstream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "subtitles.h"

struct event_info
{
    int32_t x1, x2, y1, y2;
    int duration;
    int64_t start;
    int64_t pos;
};

class SubripParser: public BaseSubtitleParser
{
    int get_event_info(const char *line, event_info *ei);
    int add_event(subtitleMap &submap, std::stringstream &buf, char *line_cache,
                     const event_info *ei, int append_cache);
public:
    SubripParser(){}
    ~SubripParser(){}
    std::string name(){return "SubripParser";}
    int _probe(std::istream &is);
    bool _parse(std::istream &is, int fps, subtitleMap &submap);
};
#endif
