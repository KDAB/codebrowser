/****************************************************************************
 * Copyright (C) 2012 Woboq UG (haftungsbeschraenkt)
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

#include "../global.h"


#include <boost/filesystem.hpp>


#include <fstream>
#include <iostream>
#include <llvm/Support/raw_ostream.h>

std::string Generator::escapeAttr(llvm::StringRef s)
{
    unsigned len = s.size();
    std::string Str;
    llvm::raw_string_ostream os(Str);

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
    return os.str();
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

void Generator::generate(const std::string &outputPrefix, std::string data_path, const std::string &filename,
                         const char *begin, const char *end,
                         const std::string &footer)
{
    std::string real_filename = outputPrefix % "/" % filename % ".html";
    boost::filesystem::create_directories(boost::filesystem::path(real_filename).parent_path());
    std::ofstream myfile;
    myfile.open(real_filename);
    if (!myfile) {
        std::cerr << "Error generating " << real_filename << std::endl;
        return;
    }

    int count = std::count(filename.begin(), filename.end(), '/');
    std::string root_path = "..";
    for (int i = 0; i < count - 1; i++)
        root_path += "/..";

    if (data_path.size() && data_path[0] == '.')
        data_path = root_path % "/" % data_path;

    myfile << "<!doctype html>\n" // Use HTML 5 doctype
    "<html>\n<head>\n";
    myfile << "<title>" << llvm::StringRef(filename).rsplit('/').second.str() << " [" << filename << "] - Woboq Code Browser</title>\n";
    myfile << "<link rel=\"stylesheet\" href=\"" << data_path << "/kdevelop.css\" title=\"KDevelop\"/>\n";
    myfile << "<link rel=\"alternate stylesheet\" href=\"" << data_path << "/qtcreator.css\" title=\"QtCreator\"/>\n";
    myfile << "<script type=\"text/javascript\" src=\"" << data_path << "/jquery/jquery.min.js\"></script>\n";
    myfile << "<script type=\"text/javascript\" src=\"" << data_path << "/jquery/jquery-ui.min.js\"></script>\n";
    myfile << "<script>var file = '"<< filename  <<"'; var root_path = '"<< root_path <<"';";
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
              "<script src='" << data_path << "/codebrowser.js'></script>\n";

    myfile << "</head>\n<body><div id='header'> </div><hr/>";

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
              "<hr/><p id='footer'>\n";

    myfile << footer;

    myfile << "<br />Powered by <a href='http://woboq.com'><img alt='Woboq' src='http://code.woboq.org/woboq-16.png' width='41' height='16' /></a> <a href='http://code.woboq.org'>Code Browser</a> "
              CODEBROWSER_VERSION "\n</p>\n</body></html>\n";
}

