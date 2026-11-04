// XmlParser.h - the launcher's small XML tokenizer/parser and file loader, used by the config reader.
#pragma once
#include "WZCommon.h"
#include <string>

struct XmlAttribute
{
    std::wstring  name;
    std::wstring  value;
    XmlAttribute* next;
    XmlAttribute() : next(nullptr) {}
};

struct XmlNode
{
    std::wstring  name;
    std::wstring  value;
    XmlNode*      next;         // circular sibling list
    XmlNode*      prev;
    XmlNode*      firstChild;
    XmlAttribute* attributes;   // most recently parsed attribute first
    XmlNode() : next(nullptr), prev(nullptr), firstChild(nullptr), attributes(nullptr) {}
};

class XmlDocument : public XmlNode
{
public:
    const wchar_t* error;

    XmlDocument() : error(nullptr) {}
    ~XmlDocument() { Clear(); }

    // /: delete all children and reset name/value.
    void Clear();

    // parse one element (or skip a <? ?> / <!- comment) at p under 'parent'.
    // Returns the pointer after what was consumed, or nullptr on failure / end of input.
    const wchar_t* ParseElement(const wchar_t* p, XmlNode* parent);

    // Drives ParseElement over the text the way does. Returns false on a parse failure.
    bool Parse(const wchar_t* text);
};

// (+ raw file read): reads a file as UTF-16LE (BOM FFFE) or UTF-8 (optional BOM).
// Returns false if the file cannot be opened.
bool XmlLoadTextFile(const wchar_t* path, std::wstring& out);
