#include <cstdlib>
#include "common.h"

Url::Url(const std::string& url):
    m_url(url),
    m_port(80)
{
    parseUrl(url);
}

void Url::parseUrl(std::string url)
{
    size_t delim_start = url.find("://");
    if (delim_start == std::string::npos)
        return;
    size_t fragment_start = url.find("#");
    if (fragment_start != std::string::npos)
    {
        m_fragment = url.substr(fragment_start + 1);
        m_url = url = url.substr(0, url.length() - m_fragment.length() - 1);
    }
    m_proto = url.substr(0, delim_start);

    std::string host, path;
    size_t path_start = url.find("/", delim_start + 3);
    if (path_start != std::string::npos)
    {
        path = url.substr(path_start);
        host = url.substr(delim_start + 3, path_start - delim_start - 3);
    }
    else
    {
        host = url.substr(delim_start);
    }
    size_t port_start = host.find(":");
    if (port_start != std::string::npos)
    {
        m_port = atoi(host.substr(port_start + 1).c_str());
        host = host.substr(0, port_start);
    }
    size_t query_start = path.find("?");
    if (query_start != std::string::npos)
    {
        m_query = path.substr(query_start+1);
        path = path.substr(0, query_start);
    }
    m_host = host;
    m_path = path;
}

