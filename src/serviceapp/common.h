#ifndef __serviceapp__common_
#define __serviceapp__common_

#ifndef NO_PYTHON
#include <Python.h>
#endif

#ifndef NO_UCHARDET
#include <uchardet/uchardet.h>
#endif

#include <algorithm>
#include <cctype>
#include <functional>
#include <locale>
#include <string>

class Url
{
    std::string m_url;
    std::string m_proto;
    std::string m_host;
    unsigned int m_port;
    std::string m_path;
    std::string m_query;
    std::string m_fragment;
    std::string url() const;
    std::string host() const;
    unsigned int port() const;
    std::string path() const;
    std::string query() const;
    std::string fragment() const;
    void parseUrl(const std::string url);

public:
    Url(const std::string &url);
    std::string url(){return m_url;}
    std::string proto(){return m_proto;}
    std::string host(){return m_host;}
    int port(){return m_port;}
    std::string path(){return m_path;}
    std::string query(){return m_query;}
    std::string fragment(){return m_fragment;}
};

void splitExtension(const std::string &path, std::string &basename, std::string &extension);

#ifndef NO_UCHARDET
int detectEncoding(const std::string &content, std::string &encoding);
#endif

#ifndef NO_PYTHON
int convertToUTF8(const std::string &input_string, const std::string &input_encoding, std::string &output_string);
int convertToUTF8(const std::string &input_string, std::string &output_string);
#endif

inline std::string &rtrim(std::string &s) 
{
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}
#endif
