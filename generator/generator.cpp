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

#include "generator.h"
#include "stringbuilder.h"
#include "filesystem.h"

#include "../global.h"

#include <fstream>
#include <iostream>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

llvm::StringRef Generator::escapeAttr(llvm::StringRef s, llvm::SmallVectorImpl< char >& buffer)
{
    llvm::raw_svector_ostream os(buffer);
    escapeAttr(os, s);
    return os.str();
}

void Generator::escapeAttr(llvm::raw_ostream &os, llvm::StringRef s)
{
    unsigned len = s.size();
    for (unsigned i = 0 ; i < len; ++i) {
        char c = s[i];
        switch (c) {
            default:
                os << c; break;

            case '<': os << "&lt;"; break;
            case '>': os << "&gt;"; break;
            case '&': os << "&amp;"; break;
            case '\"': os << "&quot;"; break;
            case '\'': os << "&apos;"; break;
        }
    }

}

void Generator::Tag::open(std::ostream &myfile) const
{
    myfile << "<" << name;
    if (!attributes.empty())
        myfile << " " << attributes;

    if (len) {
        myfile << ">";
    } else {
        myfile << "/>";
    }
}

void Generator::Tag::close(std::ostream &myfile) const
{
    myfile << "</" << name << ">";
}

void Generator::generate(llvm::StringRef outputPrefix, std::string dataPath, const std::string &filename,
                         const char* begin, const char* end, llvm::StringRef footer, llvm::StringRef warningMessage)
{
    std::string real_filename = outputPrefix % "/" % filename % ".html";
    // Make sure the parent directory exist:
    create_directories(llvm::StringRef(real_filename).rsplit('/').first);
    std::ofstream myfile;

    myfile.open(real_filename);
    if (!myfile) {
        std::cerr << "Error generating " << real_filename << std::endl;
        return;
    }

    int count = std::count(filename.begin(), filename.end(), '/');
    std::string root_path = "..";
    for (int i = 0; i < count - 1; i++) {
        root_path += "/..";
    }

    if (dataPath.size() && dataPath[0] == '.')
        dataPath = root_path % "/" % dataPath;

    myfile << "<!doctype html>\n" // Use HTML 5 doctype
    "<html>\n<head>\n";
    myfile << "<title>" << llvm::StringRef(filename).rsplit('/').second.str() << " [" << filename << "] - Woboq Code Browser</title>\n";
    myfile << "<link rel=\"stylesheet\" href=\"" << dataPath << "/kdevelop.css\" title=\"KDevelop\"/>\n";
    myfile << "<link rel=\"alternate stylesheet\" href=\"" << dataPath << "/qtcreator.css\" title=\"QtCreator\"/>\n";
    myfile << "<script type=\"text/javascript\" src=\"" << dataPath << "/jquery/jquery.min.js\"></script>\n";
    myfile << "<script type=\"text/javascript\" src=\"" << dataPath << "/jquery/jquery-ui.min.js\"></script>\n";
    myfile << "<script>var file = '"<< filename  <<"'; var root_path = '"<< root_path <<"';" << "var dataPath = '" << dataPath << "';";
    if (!projects.empty()) {
        myfile << "var projects = {";
        bool first = true;
        for (auto it: projects) {
            if (!first) myfile << ", ";
            first = false;
            myfile << "\"" << it.first << "\" : \"" << it.second <<  "\"";
        }
        myfile << "};";
    }
    myfile << "</script>\n"
              "<script src='" << dataPath << "/codebrowser.js'></script>\n";

    myfile << "</head>\n<body><div id='header'><h1 id='breadcrumb'><span>Source code of </span>";
    {
        int i = 0;
        llvm::StringRef tail = filename;
        while (i < count - 1) {
            myfile << "<a href='..";
            for (int f = 0; f < count - i - 2; ++f) {
                myfile << "/..";
            }
            auto split = tail.split('/');
            myfile << "'>" << split.first.str() << "</a>/";

            tail = split.second;
            ++i;
        }
        auto split = tail.split('/');
        myfile << "<a href='./'>" << split.first.str() << "</a>/" << split.second.str();
    }
    myfile << "</h1></div>\n<hr/><div id='content'>";

    if (!warningMessage.empty()) {
        myfile << "<p class=\"warnmsg\">";
        myfile.write(warningMessage.begin(), warningMessage.size());
        myfile << "</p>\n";
    }

    //** here we put the code
    myfile << "<table class=\"code\">\n";


    const char *c = begin;
    uint line = 1;
    const char *bufferStart = c;

    auto tags_it = tags.cbegin();
    const char *next_start = tags_it != tags.cend() ? (begin + tags_it->pos) : end;
    const char *next_end = end;
    const char *next = next_start;


    auto flush = [&]() {
        if (bufferStart != c)
            myfile.write(bufferStart, c - bufferStart);
        bufferStart = c;
    };

    myfile << "<tr><th id=\"1\">"<< 1 << "</th><td>";

    std::deque<const Tag*> stack;


    while (true) {
        if (c == next) {
            flush();
            while (!stack.empty() && c >= next_end) {
                const Tag *top = stack.back();
                stack.pop_back();
                top->close(myfile);
                next_end = end;
                if (!stack.empty()) {
                    top = stack.back();
                    next_end = begin + top->pos + top->len;
                }
            }
            if (c >= end)
                break;
            assert(c < end);
            while (c == next_start && tags_it != tags.cend()) {
                assert(c == begin + tags_it->pos);
                tags_it->open(myfile);
                if (tags_it->len) {
                    stack.push_back(&(*tags_it));
                    next_end =  c + tags_it->len;
                }

                tags_it++;
                next_start = tags_it != tags.cend() ? (begin + tags_it->pos) : end;
            };

            next = std::min(next_end, next_start);
            //next = std::min(end, next);
        }

        switch (*c) {
            case '\n':
                flush();
                ++bufferStart; //skip the new line
                ++line;
                for (auto it = stack.crbegin(); it != stack.crend(); ++it)
                    (*it)->close(myfile);
                myfile << "</td></tr>\n"
                          "<tr><th id=\"" << line << "\">"<< line << "</th><td>";
                for (auto it = stack.cbegin(); it != stack.cend(); ++it)
                     (*it)->open(myfile);
                break;
            case '&': flush(); ++bufferStart; myfile << "&amp;"; break;
            case '<': flush(); ++bufferStart; myfile << "&lt;"; break;
            case '>': flush(); ++bufferStart; myfile << "&gt;"; break;
            default: break;
        }
        ++c;
    }


    myfile << "</td></tr>\n"
              "</table>"
              "<hr/>";

    if (!warningMessage.empty()) {
        myfile << "<p class=\"warnmsg\">";
        myfile.write(warningMessage.begin(), warningMessage.size());
        myfile << "</p>\n";
    }

    myfile << "<p id='footer'>\n";

    myfile.write(footer.begin(), footer.size());

    myfile << "<br />Powered by <a href='http://woboq.com'><img alt='Woboq' src='http://code.woboq.org/woboq-16.png' width='41' height='16' /></a> <a href='http://code.woboq.org'>Code Browser</a> "
              CODEBROWSER_VERSION "\n</p>\n</div></body></html>\n";
}

