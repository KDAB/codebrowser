/****************************************************************************
 * Copyright (C) 2012-2014 Woboq GmbH
 * Olivier Goffart <contact at woboq.com>
 * http://woboq.com/codebrowser.html
 *
 * This file is part of the Woboq Code Browser.
 *
 * Commercial License Usage:
 * Licensees holding valid commercial licenses provided by Woboq may use
 * this file in accordance with the terms contained in a written agreement
 * between the licensee and Woboq.
 * For further information see http://woboq.com/codebrowser.html
 *
 * Alternatively, this work may be used under a Creative Commons
 * Attribution-NonCommercial-ShareAlike 3.0 (CC-BY-NC-SA 3.0) License.
 * http://creativecommons.org/licenses/by-nc-sa/3.0/deed.en_US
 * This license does not allow you to use the code browser to assist the
 * development of your commercial software. If you intent to do so, consider
 * purchasing a commercial licence.
 ****************************************************************************/

#pragma once

#include <string>
#include <set>
#include <map>
#include <llvm/ADT/SmallString.h>

namespace llvm {
class raw_ostream;
}


/* This class generate the HTML out of a file with the said tags.
 */
class Generator {

    struct Tag {
        std::string name;
        std::string attributes;
        int pos;
        int len;
        bool operator<(const Tag &other) const {
            // Ordered first by position, and then by lenth (reverse order)
            return (pos != other.pos) ? pos < other.pos : len > other.len;
        }
        void open(std::ostream& myfile) const;
        void close(std::ostream& myfile) const;
    };

    std::set<Tag> tags;

    std::map<std::string, std::string> projects;

public:

    void addTag(std::string name, std::string attributes, int pos, int len) {
        tags.insert({std::move(name), std::move(attributes), pos, len});
    }
    void addProject(std::string a, std::string b) {
        projects.insert({std::move(a), std::move(b) });
    }

    void generate(const std::string &outputPrefix, std::string dataPath, const std::string &filename,
                  const char *begin, const char *end,
                  const std::string &footer);

    static llvm::StringRef escapeAttr(llvm::StringRef, llvm::SmallVectorImpl<char> &buffer);
    static void escapeAttr(llvm::raw_ostream& os, llvm::StringRef s);
};
