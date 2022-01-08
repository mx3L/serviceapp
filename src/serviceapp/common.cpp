#include <dirent.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <stdint.h>
#include <string>
#include <sys/stat.h>

#include "common.h"
#include <Python.h>

SettingEntry::SettingEntry():m_is_set(false){}
SettingEntry::SettingEntry(const std::string &app_arg, int value, const std::string value_type):
    m_is_set(true),
    m_app_arg(app_arg), m_int_value(value), m_value_type(value_type){}
SettingEntry::SettingEntry(const std::string &app_arg, const std::string &value, const std::string value_type):
    m_is_set(true),
    m_app_arg(app_arg), m_string_value(value), m_value_type(value_type){}
SettingEntry::SettingEntry(const std::string &app_arg, const std::string value_type):
    m_is_set(false),
    m_app_arg(app_arg), m_value_type(value_type){}

void SettingEntry::setValue(int value)
{
    m_is_set = true;
    m_int_value = value;
}

void SettingEntry::setValue(std::string value)
{
    if (value.empty())
        return;
    m_is_set = true;
    m_string_value = value;
}

std::string SettingEntry::getAppArg() const
{
    return m_app_arg;
}

std::string SettingEntry::getValue() const
{
    if (m_value_type == "int" || m_value_type == "bool")
    {
        std::stringstream ss;
        ss << m_int_value;
        return ss.str();
    }
    return m_string_value;
}

int SettingEntry::getValueInt() const { return m_int_value; }

std::string SettingEntry::toString() const
{
    std::stringstream ss;
    if (!m_is_set)
    {
        ss << "not set";
    }
    else
    {
        ss << getValue();
    }
    return ss.str();
}

bool SettingEntry::isSet() const {return m_is_set;}
std::string SettingEntry::getType() const { return m_value_type; }

Url::Url(const std::string& url):
    m_url(url),
    m_port(-1)
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

void splitExtension(const std::string &path, std::string &basename, std::string &extension)
{
    size_t filename_idx = path.find_last_of('/');
    size_t extension_idx = path.find_last_of('.');
    bool has_extension = (extension_idx != std::string::npos
            && (filename_idx == std::string::npos || extension_idx > filename_idx));
    if (has_extension)
    {
        basename = path.substr(0, extension_idx);
        extension = path.substr(extension_idx);
    }
    else
    {
        basename = path;
        extension = "";
    }
}

void splitPath(const std::string &path, std::string &dirpath, std::string &filename)
{
    size_t filename_idx = path.find_last_of('/');
    if (filename_idx != std::string::npos)
    {
        dirpath = path.substr(0, filename_idx);
        filename = path.substr(filename_idx + 1);
    }
    else
    {
        dirpath = "";
        filename = path;
    }
}

int listDir(const std::string &dirpath, std::vector<std::string> *files, std::vector<std::string> *directories)
{
    DIR *dp;
    if ((dp = opendir(dirpath.c_str())) == NULL)
    {
        fprintf(stderr, "listDir(%s) - error in opendir: %m\n",
                dirpath.c_str());
        return -1;
    }

    std::string filepath;
    struct dirent *entry;
    struct stat statbuf;
    while ((entry = readdir(dp)) != NULL)
    {
        if (*dirpath.rbegin() == '/')
            filepath = dirpath + entry->d_name;
        else
            filepath = dirpath + "/" + entry->d_name;
        stat(filepath.c_str(), &statbuf);
        if (S_ISDIR(statbuf.st_mode))
        {
            if (!strcmp("..", entry->d_name) || !strcmp(".", entry->d_name))
            {
                continue;
            }
            if (directories != NULL)
            {
                directories->push_back(entry->d_name);
            }
        }
        else
        {
            if (files != NULL)
            {
                files->push_back(entry->d_name);
            }
        }
    }
    if (closedir(dp) == -1)
    {
        fprintf(stderr, "listDir(%s) - error in closedir: %m\n", dirpath.c_str());
    }
    return 0;
}

static const uint8_t iso8859_2_unused_utf8[10][2] = {
    {0xc2,0x8a},{0xc2,0x8c},{0xc2,0x8d},{0xc2,0x8e},{0xc2,0x8f},
    {0xc2,0x9a},{0xc2,0x9c},{0xc2,0x9d},{0xc2,0x9e},{0xc2,0x9f}};


#ifndef NO_UCHARDET
int detectEncoding(const std::string &content, std::string &encoding)
{
    uchardet_t handle = uchardet_new();
    int retval = uchardet_handle_data(handle, content.c_str(), content.length());
    if (retval != 0)
    {
        fprintf(stderr, "uchardet error: handle data error.\n");
        return 1;
    }
    uchardet_data_end(handle);
    encoding = (uchardet_get_charset(handle));
    uchardet_delete(handle);
    return 0;
}
#endif

#ifndef NO_PYTHON
int convertToUTF8(const std::string &input_string, const std::string &input_encoding, std::string &output_string)
{
    PyObject *py_string, *py_unicode;
#if PY_MAJOR_VERSION >= 3
    py_string = PyUnicode_FromStringAndSize(input_string.c_str(), input_string.length());
#else
    py_string = PyString_FromStringAndSize(input_string.c_str(), input_string.length());
#endif
    if (py_string == NULL)
    {
        PyErr_Print();
        return 1;
    }
#if PY_MAJOR_VERSION >= 3
    py_unicode = PyUnicode_AsDecodedObject(py_string, input_encoding.c_str(), "strict");
#else
    py_unicode = PyString_AsDecodedObject(py_string, input_encoding.c_str(), "strict");
#endif
    if (py_unicode == NULL)
    {
        Py_DECREF(py_string);
        PyErr_Print();
        return 1;
    }
    Py_DECREF(py_string);
    py_string = PyUnicode_AsUTF8String(py_unicode);
    if (py_string == NULL)
    {
        Py_DECREF(py_unicode);
        PyErr_Print();
        return 1;
    }
    Py_DECREF(py_unicode);
#if PY_MAJOR_VERSION >= 3
    output_string = PyUnicode_AsUTF8(py_string);
#else
    output_string = PyString_AsString(py_string);
#endif
    Py_DECREF(py_string);
    return 0;
}

int convertToUTF8(const std::string &input_string, std::string &output_string)
{
    std::string input_encoding;
    if (detectEncoding(input_string, input_encoding) != 0)
    {
        fprintf(stderr, "convertToUTF8 - cannot detect encoding\n");
        return -1;
    }
    fprintf(stderr, "convertToUTF8 - detected input encoding: %s\n", input_encoding.c_str());
    if (convertToUTF8(input_string, input_encoding, output_string) != 0)
    {
        fprintf(stderr, "convertToUTF8 - cannot convert to utf-8");
        return -1;
    }
    // workaround when uchardet detects wrongly ISO-8859-2 instead
    // of WINDOWS-1250
    if (input_encoding == "ISO-8859-2")
    {
        bool decode_again = false;
        for (int i = 0; i < 10; i++)
        {
            fprintf(stderr, "convertToUTF8 - looking for %#x,%#x: ", iso8859_2_unused_utf8[i][0], iso8859_2_unused_utf8[i][1]);
            void *ptr = memmem(output_string.c_str(), output_string.length(), iso8859_2_unused_utf8[i], 2);
            if (ptr != NULL)
            {
                fprintf(stderr, "found\n");
                decode_again = true;
                break;
            }
            printf("not found\n");
        }
        if (decode_again)
        {
            fprintf(stderr, "convertToUTF8 - ISO-8859-2 is not right encoding, trying WINDOWS-1250\n");
            if (0 != convertToUTF8(input_string, "WINDOWS-1250", output_string))
            {
                fprintf(stderr, "convertToUTF8 - cannot convert to utf-8");
                return -1;
            }
        }
    }
    return 0;
}
#endif

static int unquotePlus(char* out, const char* in);

HeaderMap getHeaders(const std::string& url)
{
    std::map<std::string, std::string> headers;
    size_t pos = url.find('#');
    if (pos != std::string::npos && (url.compare(0, 4, "http") == 0 || url.compare(0, 4, "rtsp") == 0))
    {
        std::string headers_str = url.substr(pos + 1);
        char *headers_cstr = (char*) malloc ((url.length() + 1) * sizeof(char));
        if (!unquotePlus(headers_cstr, headers_str.c_str()))
        {
            headers_str = headers_cstr;
        }
        else
        {
            fprintf(stderr, "getHeaders - cannot unquote headers string\n");
        }
        free(headers_cstr);

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

// http://stackoverflow.com/questions/2673207/c-c-url-decode-library
static int unquotePlus(char* out, const char* in)
{
    static const signed char tbl[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
         0, 1, 2, 3, 4, 5, 6, 7,  8, 9,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1
    };
    char c, v1, v2;
    char *beg = out;
    if(in != NULL) {
        while((c=*in++) != '\0') {
            if(c == '+')
            {
                *out++ = ' ';
                continue;
            }
            if(c == '%') {
                if(!(v1=*in++) || (v1=tbl[(unsigned char)v1])<0 ||
                   !(v2=*in++) || (v2=tbl[(unsigned char)v2])<0) {
                    *beg = '\0';
                    return -1;
                }
                c = (v1<<4)|v2;
            }
            *out++ = c;
        }
    }
    *out = '\0';
    return 0;
}

