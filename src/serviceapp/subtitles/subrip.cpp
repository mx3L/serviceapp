// adapted ffmpeg srtdec sources:
// https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/srtdec.c

#include "subrip.h"
#include "common.h"

int SubripParser::get_event_info(const char *line, event_info *ei)
{
    int hh1, mm1, ss1, ms1;
    int hh2, mm2, ss2, ms2;

    ei->x1 = ei->x2 = ei->y1 = ei->y2 = ei->duration = -1;
    ei->start = 0;//fixme
    ei->pos = -1;
    if (sscanf(line, "%d:%2d:%2d%*1[,.]%3d --> %d:%2d:%2d%*1[,.]%3d"
               "%*[ ]X1:%u X2:%u Y1:%u Y2:%u",
               &hh1, &mm1, &ss1, &ms1,
               &hh2, &mm2, &ss2, &ms2,
               &ei->x1, &ei->x2, &ei->y1, &ei->y2) >= 8) {
        const int64_t start = (hh1*3600LL + mm1*60LL + ss1) * 1000LL + ms1;
        const int64_t end   = (hh2*3600LL + mm2*60LL + ss2) * 1000LL + ms2;
        ei->duration = end - start;
        ei->start = start;
        return 0;
    }
    return -1;
}

int SubripParser::add_event(subtitleMap &submap, std::stringstream &buf, char *line_cache,
                     const event_info *ei, int append_cache)
{
    if (append_cache && line_cache[0])
        buf << line_cache << std::endl;
    line_cache[0] = 0;

    subtitleMessage sub;
    sub.start_ms = ei->start;
    sub.duration_ms = ei->duration;
    sub.end_ms = sub.start_ms + sub.duration_ms;
    sub.text = buf.str();
    sub.text = rtrim(sub.text);

    submap.insert(std::pair<int, subtitleMessage>(sub.end_ms, sub));
    return 0;
}

bool SubripParser::_parse(std::istream &is, int fps, subtitleMap &submap)
{
    std::stringstream buf;
    int res = 0;
    char line[4096], line_cache[4096];
    int has_event_info = 0;
    event_info ei;
    while (is)
    {
        event_info tmp_ei;
        is.getline(line, sizeof(line));
        int len = strlen(line);

        if (len < 0)
            break;

        if (!len || !line[0] || (len == 1 && line[0] == '\r') )
            continue;

        if (get_event_info(line, &tmp_ei) < 0)
        {
            char *pline;

            if (!has_event_info)
                continue;

            if (line_cache[0]) {
                /* We got some cache and a new line so we assume the cached
                 * line was actually part of the payload */
                buf << line_cache << std::endl;
                line_cache[0] = 0;
            }

            /* If the line doesn't start with a number, we assume it's part of
             * the payload, otherwise is likely an event number preceding the
             * timing information... but we can't be sure of this yet, so we
             * cache it */
            if (strtol(line, &pline, 10) < 0 || line == pline)
                buf << line << std::endl;
            else
                strcpy(line_cache, line);
        }
        else
        {
            if (has_event_info)
            {
                /* We have the information of previous event, append it to the
                 * queue. We insert the cached line if and only if the payload
                 * is empty and the cached line is not a standalone number. */
                char *pline = NULL;
                const int standalone_number = strtol(line_cache, &pline, 10) >= 0 && pline && !*pline;
                int buflen = buf.str().size();

                res = add_event(submap, buf, line_cache, &ei, !buflen && !standalone_number);
                buf.str(std::string());
                line_cache[0] = 0;
                if (res < 0)
                    goto end;
            } else {
                has_event_info = 1;
            }
            //tmp_ei.pos = pos;
            ei = tmp_ei;
        }
    }
    if (has_event_info) {
        res = add_event(submap, buf, line_cache, &ei, 1);
        if (res < 0)
            goto end;
    }
end:
    if (res < 0)
        return false;
    return true;

}

int SubripParser::_probe(std::istream &is)
{
    int v;
    char buf[64], *pbuf;

    while (is.peek() == '\r' || is.peek() == '\n')
        is.read(buf, 1);

    /* Check if the first non-empty line is a number. We do not check what the
     * number is because in practice it can be anything.
     * Also, that number can be followed by random garbage, so we can not
     * unfortunately check that we only have a number. */
    if (!is.getline(buf, sizeof(buf)) || strtol(buf, &pbuf, 10) < 0 || pbuf == buf)
        return PROB_SCORE_MIN;

    /* Check if the next line matches a SRT timestamp */
    if (!is.getline(buf, sizeof(buf)))
        return PROB_SCORE_MIN;

    if (buf[0] >= '0' && buf[0] <= '9' && strstr(buf, " --> ")
        && sscanf(buf, "%*d:%*2d:%*2d%*1[,.]%*3d --> %*d:%*2d:%*2d%*1[,.]%3d", &v) == 1)
        return PROB_SCORE_MAX;
    return PROB_SCORE_MIN;
}

