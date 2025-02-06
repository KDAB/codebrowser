/****************************************************************************
 * Copyright (C) 2012-2016 Woboq GmbH
 * Olivier Goffart <contact at woboq.com>
 * https://woboq.com/codebrowser.html
 *
 * This file is part of the Woboq Code Browser.
 *
 * Commercial License Usage:
 * Licensees holding valid commercial licenses provided by Woboq may use
 * this file in accordance with the terms contained in a written agreement
 * between the licensee and Woboq.
 * For further information see https://woboq.com/codebrowser.html
 *
 * Alternatively, this work may be used under a Creative Commons
 * Attribution-NonCommercial-ShareAlike 3.0 (CC-BY-NC-SA 3.0) License.
 * http://creativecommons.org/licenses/by-nc-sa/3.0/deed.en_US
 * This license does not allow you to use the code browser to assist the
 * development of your commercial software. If you intent to do so, consider
 * purchasing a commercial licence.
 ****************************************************************************/

#pragma once

#include <llvm/ADT/SmallString.h>
#include <map>
#include <set>
#include <string>

namespace llvm {
class raw_ostream;
}


/* This class generate the HTML out of a file with the said tags.
 */
class Generator
{

    struct Tag
    {
        std::string name;
        std::string attributes;
        int pos;
        int len;
        std::string innerHtml;
        bool operator<(const Tag &other) const
        {
            // This is the order of the opening tag. Order first by position, then by length
            //  (in the reverse order) with the exception of length of 0 which always goes first.
            //  Ordered first by position, and then by lenth (reverse order)
            return (pos != other.pos) ? pos < other.pos
                                      : len == 0 || (other.len != 0 && len > other.len);
        }
        bool operator==(const Tag &other) const
        {
            return std::tie(pos, len, name, attributes)
                == std::tie(other.pos, other.len, other.name, other.attributes);
        }
        void open(llvm::raw_ostream &myfile) const;
        void close(llvm::raw_ostream &myfile) const;
    };

    std::multiset<Tag> tags;

    std::map<std::string, std::string> projects;

public:
    void addTag(std::string name, std::string attributes, int pos, int len,
                std::string innerHtml = {})
    {
        if (len < 0) {
            return;
        }
        Tag t = { std::move(name), std::move(attributes), pos, len, std::move(innerHtml) };
        auto it = tags.find(t);
        if (it != tags.end() && *it == t)
            return; // Hapens in macro for example
        tags.insert(std::move(t));
    }
    void addProject(std::string a, std::string b)
    {
        projects.insert({ std::move(a), std::move(b) });
    }

    void generate(llvm::StringRef outputPrefix, std::string dataPath, const std::string &filename,
                  const char *begin, const char *end, llvm::StringRef footer,
                  llvm::StringRef warningMessage,
                  const std::set<std::string> &interestingDefitions);

    static llvm::StringRef escapeAttr(llvm::StringRef, llvm::SmallVectorImpl<char> &buffer);

    /**
     * This function takes a reference name that was already processed by `escapeAttr` and
     * replaces additional chars which are not allowed in filenames on all platforms.
     *
     * @param s already escaped ref
     * @param buffer
     * @return
     */
    static llvm::StringRef escapeAttrForFilename(llvm::StringRef s,
                                                 llvm::SmallVectorImpl<char> &buffer);
    static void escapeAttr(llvm::raw_ostream &os, llvm::StringRef s);

    struct EscapeAttr
    {
        llvm::StringRef value;
    };
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Generator::EscapeAttr s)
{
    Generator::escapeAttr(os, s.value);
    return os;
}
