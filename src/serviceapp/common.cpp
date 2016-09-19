#include <cstdlib>
#include <cstdio>
#include <string>
#include <stdint.h>

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
    py_string = PyString_FromStringAndSize(input_string.c_str(), input_string.length());
    if (py_string == NULL)
    {
        PyErr_Print();
        return 1;
    }
    py_unicode = PyString_AsDecodedObject(py_string, input_encoding.c_str(), "strict");
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
    output_string = PyString_AsString(py_string);
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

