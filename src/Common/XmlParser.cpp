#include "XmlParser.h"
#include <stdio.h>
#include <wchar.h>

namespace
{
    // skip whitespace; "<...>" tag token or text token up to the next '<'.
    // Returns the start (leading whitespace included) and the token length including that whitespace.
    const wchar_t* NextToken(const wchar_t* p, int* len)
    {
        int i = 0;
        if (!p[0])
            return nullptr;
        while (p[i] <= 0x20)
        {
            ++i;
            if (!p[i])
                return nullptr;
        }
        if (p[i] == L'<')
        {
            int n = 1;
            if (p[i + 1] == L'>')
            {
                *len = n + 1;
                return p + i;
            }
            const wchar_t* q = p + i + 1;
            while (*q)
            {
                ++q;
                ++n;
                if (*q == L'>')
                {
                    *len = n + 1;
                    return p + i;
                }
            }
            return nullptr;
        }
        int n = 1;
        if (p[i + 1] == L'<')
        {
            *len = i + n;
            return p;
        }
        const wchar_t* q = p + i + 1;
        while (*q)
        {
            ++q;
            ++n;
            if (*q == L'<')
            {
                *len = i + n;
                return p;
            }
        }
        return nullptr;
    }

    // tokenizer inside a tag. Quoted strings (pointer is after the quote, len = content,
    // advance = content + closing quote), single '/' or '=' tokens, or bare words.
    const wchar_t* NextAttrToken(const wchar_t* p, int n, int* len, int* advance)
    {
        int i = 0;
        if (n <= 0)
            return nullptr;
        while (p[i] <= 0x20)
        {
            if (++i >= n)
                return nullptr;
        }
        const wchar_t* t = p + i;
        if (*t == L'"')
        {
            const wchar_t* r = t + 1;
            int c = 0;
            if (t[1] != L'"')
            {
                int idx = i + 1;
                const wchar_t* q = t + 1;
                for (;;)
                {
                    if (idx > n)
                        return nullptr;
                    ++q;
                    ++c;
                    ++idx;
                    if (*q == L'"')
                        break;
                }
            }
            *len = c;
            *advance = c + 1;
            return r;
        }
        if (wcschr(L"/=", *t))
        {
            *len = 1;
            *advance = 1;
            return t;
        }
        int c = 0;
        if (*t > 0x20)
        {
            int idx = i;
            const wchar_t* q = t;
            do
            {
                if (wcschr(L"/=", *q))
                    break;
                if (idx >= n)
                    break;
                ++q;
                ++c;
                ++idx;
            } while (*q > 0x20);
        }
        *len = c;
        *advance = c;
        return t;
    }

    void FreeChildren(XmlNode* node);

    void FreeNode(XmlNode* node)
    {
        XmlAttribute* a = node->attributes;
        while (a)
        {
            XmlAttribute* nx = a->next;
            delete a;
            a = nx;
        }
        if (node->firstChild)
            FreeChildren(node->firstChild);
        delete node;
    }

    // free a sibling ring starting at 'first'.
    void FreeChildren(XmlNode* first)
    {
        XmlNode* n = first;
        do
        {
            XmlNode* nx = n->next;
            FreeNode(n);
            n = nx;
        } while (n && n != first);
    }

    // parse "name attr=value ... [/]" from a tag body of n chars. Sets selfClose when '/' is seen.
    XmlNode* ParseTag(const wchar_t* body, int n, bool* selfClose)
    {
        *selfClose = false;
        int len = 0, adv = 0;
        const wchar_t* tok = NextAttrToken(body, n, &len, &adv);
        XmlNode* node = new XmlNode();
        if (tok)
            node->name.assign(tok, len);
        const wchar_t* cur = tok ? tok + adv : nullptr;
        const wchar_t* end = body + n;

        while (cur)
        {
            const wchar_t* nameTok = NextAttrToken(cur, (int)(end - cur), &len, &adv);
            if (!nameTok)
                return node;
            if (len == 1 && *nameTok == L'/')
            {
                *selfClose = true;
                return node;
            }
            XmlAttribute* attr = new XmlAttribute();
            attr->name.assign(nameTok, len);

            const wchar_t* eq = NextAttrToken(nameTok + adv, (int)(end - (nameTok + adv)), &len, &adv);
            if (!eq || *eq != L'=')
            {
                delete attr;
                delete node;
                return nullptr;
            }
            const wchar_t* valTok = NextAttrToken(eq + adv, (int)(end - (eq + adv)), &len, &adv);
            if (valTok)
                attr->value.assign(valTok, len);
            attr->next = node->attributes;
            node->attributes = attr;
            cur = valTok ? valTok + adv : nullptr;
        }
        return node;
    }
}

void XmlDocument::Clear()
{
    while (firstChild)
    {
        XmlNode* n = firstChild;
        XmlNode* nx = n->next;
        if (firstChild == n)
            firstChild = nx;
        if (nx == n)
        {
            firstChild = nullptr;
        }
        else
        {
            n->prev->next = nx;
            nx->prev = n->prev;
        }
        n->prev = nullptr;
        n->next = nullptr;
        FreeChildren(n);
    }
    value.clear();
    name.clear();
}

const wchar_t* XmlDocument::ParseElement(const wchar_t* p, XmlNode* parent)
{
    int tokLen = 0;
    const wchar_t* tok = NextToken(p, &tokLen);
    if (!tok)
        return nullptr;
    if (*tok != L'<')
    {
        error = L"Syntax error";
        return nullptr;
    }

    // "<?" processing instruction or "<!-" comment: skip.
    if ((tok[1] == L'?') || (tok[1] == L'!' && tok[2] == L'-'))
        return tok + tokLen;

    bool selfClose = false;
    XmlNode* node = ParseTag(tok + 1, tokLen - 2, &selfClose);
    if (!node)
        return nullptr;
    const wchar_t* result = tok + tokLen;

    if (parent->firstChild)
    {
        XmlNode* first = parent->firstChild;
        node->next = first;
        node->prev = first->prev;
        first->prev->next = node;
        first->prev = node;
    }
    else
    {
        parent->firstChild = node;
        node->prev = node;
        node->next = node;
    }

    if (selfClose)
        return result;

    int len = 0;
    const wchar_t* cur = NextToken(result, &len);
    if (!cur)
        return nullptr;
    if (*cur == L'<')
    {
        node->value.clear();
    }
    else
    {
        node->value.assign(cur, len);
        cur = NextToken(cur + len, &len);
    }

    if (!cur)
        return nullptr;
    while (cur[0] != L'<' || cur[1] != L'/')
    {
        const wchar_t* after = ParseElement(cur, node);
        if (!after)
            return nullptr;
        cur = NextToken(after, &len);
        if (!cur)
            return nullptr;
    }

    // closing tag "</name>"
    int closeLen = len;
    int cl = 0, adv = 0;
    const wchar_t* nameTok = NextAttrToken(cur + 2, closeLen - 3, &cl, &adv);
    std::wstring closeName;
    if (nameTok)
        closeName.assign(nameTok, cl);
    if (_wcsicmp(node->name.c_str(), closeName.c_str()) == 0)
        return cur + closeLen;
    return nullptr;
}

bool XmlDocument::Parse(const wchar_t* text)
{
    const wchar_t* p = text;
    do
        p = ParseElement(p, this);
    while (p && *p);
    return p != nullptr;
}

// +
bool XmlLoadTextFile(const wchar_t* path, std::wstring& out)
{
    FILE* f = nullptr;
    _wfopen_s(&f, path, L"rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string raw((size_t)size, '\0');
    if (size)
        fread(&raw[0], 1, (size_t)size, f);
    fclose(f);

    const unsigned char* b = (const unsigned char*)raw.data();
    out.clear();
    if (size >= 2 && b[0] == 0xFF && b[1] == 0xFE)
    {
        out.assign((const wchar_t*)(b + 2), (size_t)(size - 2) / 2);
        return true;
    }

    int i = (size >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) ? 3 : 0;
    // Hand-rolled decoder: no surrogate support, 3-byte maximum.
    while (i < size)
    {
        unsigned c = b[i];
        if ((c & 0x80) == 0)
        {
            out.push_back((wchar_t)c);
            i += 1;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            unsigned c1 = i + 1 < size ? b[i + 1] : 0;
            out.push_back((wchar_t)(((c & 0x1F) << 6) | (c1 & 0x3F)));
            i += 2;
        }
        else
        {
            unsigned c1 = i + 1 < size ? b[i + 1] : 0;
            unsigned c2 = i + 2 < size ? b[i + 2] : 0;
            out.push_back((wchar_t)(((((c << 6) | (c1 & 0x3F)) << 6) | (c2 & 0x3F)) & 0xFFFF));
            i += 3;
        }
    }
    return true;
}
