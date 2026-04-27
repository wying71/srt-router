#ifndef STRING_UTILS_H
#define STRING_UTILS_H
#include <string>
#include <locale>
#include <vector>
#include <algorithm>
#include <locale>
#include <codecvt>
#include <sstream>

/*
* String utils by CodeLive@2021-10-29
*/

template<typename T>
int stringFind(const T &source, int offset, const T &str, bool caseSensitive)
{
    if((!source.empty()) && (!str.empty()) && (offset >= 0) && (offset < (int)source.size()))
    {
        const auto loc = std::locale();
        typename T::const_iterator iter = std::search(source.begin() + offset, source.end(), str.begin(), str.end(),
            [&](const typename T::value_type a, const typename T::value_type b)
        {
            return caseSensitive ? (a == b) : (std::toupper(a, loc) == std::toupper(b, loc));
        });
        if(iter != source.end())
        {
            return (int)(iter - source.begin());
        }
    }
    return -1;
}

template<typename T>
int stringReplace(T &source, int offset, const T &from, const T &to, bool caseSensitive)
{
    int count = 0;
    int pos = 0;
    while((pos = stringFind(source, pos, from, caseSensitive)) >= 0)
    {
        source.replace(pos, from.size(), to);
        pos += (int)to.size();
        count ++;
    }
    return count;
}

template<typename T>
T stringSubstr(const T &source, int offset, const T &left, const T &right, bool caseSensitive)
{
    T ret;
    int pos1 = stringFind(source, offset, left, caseSensitive);
    if(pos1 >= 0)
    {
        int posLeft = pos1 + (int)left.size();
        int pos2 = stringFind(source, posLeft, right, caseSensitive);
        if(pos2 >= posLeft)
        {
            ret = source.substr(posLeft, pos2 - posLeft);
        }
    }
    return ret;
}

template<typename T>
T stringSubstr(const T &source, int offset, const T &left, bool caseSensitive)
{
    T ret;
    int pos1 = stringFind(source, offset, left, caseSensitive);
    if(pos1 >= 0)
    {
        int posLeft = pos1 + (int)left.size();
        ret = source.substr(posLeft);
    }
    return ret;
}

template<typename T>
void stringTrimLeft(T &value)
{
    if(value.empty())
    {
        return ;
    }
    const auto loc = std::locale();
    size_t i = 0;
    for(i = 0; i < value.length(); i ++)
    {
        if(!std::isspace(value[i], loc))
        {
            break ;
        }
    }
    value = value.substr(i);
}

template<typename T>
void stringTrimRight(T &value)
{
    if(value.empty())
    {
        return ;
    }
    const auto loc = std::locale();
    int i = 0;
    for(i = ((int)value.length()) - 1; i >= 0; i --)
    {
        if(!std::isspace(value[i], loc))
        {
            break ;
        }
    }
    value = value.substr(0, i + 1);
}

template<typename T>
void stringTrim(T &value)
{
    if(value.empty())
    {
        return ;
    }
    stringTrimLeft(value);
    stringTrimRight(value);
}

template<typename T>
void stringToLower(T &value)
{
    const auto loc = std::locale();
    std::transform(value.begin(), value.end(), value.begin(),
        [&](const typename T::value_type c)
    {
        return std::tolower(c, loc);
    });
}

template<typename T>
void stringToUpper(T &value)
{
    const auto loc = std::locale();
    std::transform(value.begin(), value.end(), value.begin(),
        [&](const typename T::value_type c)
    {
        return std::toupper(c, loc);
    });
}

template<typename T>
std::vector<T> stringSplit(const T &source, const T &separator, bool caseSensitive, bool trimSpace, bool includeEmpty)
{
    std::vector<T> strArray;
    if(source.empty() || separator.empty())
    {
        return strArray;
    }
    auto addStr = [&](T &str)
    {
        if(trimSpace)
        {
            stringTrim(str);
        }
        if((!str.empty()) || includeEmpty)
        {
            strArray.push_back(str);
        }
    };
    int size = (int)source.size();
    int offset = 0;
    while(offset <= size)
    {
        int pos = stringFind(source, offset, separator, caseSensitive);
        if(pos < 0)
        {
            T strSub;
            if(offset < size)
            {
                strSub = source.substr(offset);
            }
            addStr(strSub);
            break ;
        }
        else
        {
            T strSub = source.substr(offset, pos - offset);
            addStr(strSub);
        }
        offset = pos + (int)separator.size();
    }
    return strArray;
}

inline std::string wstring2utf8(const wchar_t* source)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(source);
}

inline std::wstring utf82wstring(const char* source)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t> > conv;
    return conv.from_bytes(source);
}

inline std::string wstring2utf8(const std::wstring& source)
{
    return wstring2utf8(source.c_str());
}

inline std::wstring utf82wstring(const std::string& source)
{
    return utf82wstring(source.c_str());
}
/*
inline std::string wstring2string(const wchar_t* source, const char* locName = "")
{
    std::locale loc(locName);
    std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> conv(
        new std::codecvt_byname<wchar_t, char, mbstate_t>(loc.name()));
    return conv.to_bytes(source);
}

inline std::wstring string2wstring(const char* source, const char* locName = "")
{
    std::locale loc(locName);
    std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> conv(
        new std::codecvt_byname<wchar_t, char, mbstate_t>(loc.name()));
    return conv.from_bytes(source);
}

inline std::string wstring2string(const std::wstring& source)
{
    return wstring2string(source.c_str());
}

inline std::wstring string2wstring(const std::string& source)
{
    return string2wstring(source.c_str());
}
*/
#endif // STRING_UTILS_H
