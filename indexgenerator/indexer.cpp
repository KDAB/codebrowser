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


#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <vector>
#include <map>

#include <boost/algorithm/string.hpp>

#include "../global.h"



struct FolderInfo {
//    std::string name;
    std::map<std::string, std::shared_ptr<FolderInfo>> subfolders;
};

void gererateRecursisively(FolderInfo *folder, const std::string &root, const std::string &path, const std::string &rel = "") {
    std::ofstream myfile;
    std::string filename = root + "/" + path + "index.html";
    myfile.open(filename);
    if (!myfile) {
        std::cerr << "Error generating " << filename << std::endl;
        return;
    }

    std::string data_path = rel + "../data";

    std::vector<std::string> parts;
    boost::split(parts, root, boost::is_any_of("/"));
    std::string project = parts.at(parts.size()-1);

    myfile << "<!doctype html>\n"
              "<head><title>Index of " << path << " - Woboq Code Browser</title>"
              "<link rel=\"stylesheet\" href=\"" << data_path << "/indexstyle.css\"/>\n";
    myfile << "<script type=\"text/javascript\" src=\"" << data_path << "/jquery/jquery.min.js\"></script>\n";
    myfile << "<script type=\"text/javascript\" src=\"" << data_path << "/jquery/jquery-ui.min.js\"></script>\n";
    myfile << "<script>var path = '"<< path <<"'; var root_path = '"<< rel <<"'; var project='"<< project <<"'; </script>\n"
              "<script src='" << data_path << "/indexscript.js'></script>\n"
              "</head>\n<body>\n";
    myfile << "<h1><a href='http://code.woboq.org'>Woboq Code Browser</a></h1>\n";
    myfile << "<p><input id='searchline' placeholder='Search for a file'  type='text'/></p>\n";
    myfile << "<h2> Index of <em>" << project << "/" << path << "</em></h2>\n";
    myfile << "<hr/><table id='tree'>\n";

    //if (!path.empty())
    {
        myfile << " <tr><td class='parent'>    <a href='../'>../</a></td></tr>\n";
    }

    for (auto it : folder->subfolders) {
        const std::string &name = it.first;
        if (it.second) {
            gererateRecursisively(it.second.get(), root, path+name+"/", rel + "../");
            myfile << "<tr><td class='folder'><a href='"<< name <<"/' class='opener' data-path='" << path << name << "'>[+]</a> "
                      "<a href='" << name << "/'>" << name << "/</a></td></tr>\n";
        } else {
            myfile << "<tr><td class='file'>    <a href='" << name << ".html'>"
                   << name
                   << "</a></td></tr>\n";
        }
    }
    myfile << "</table>"
            "<hr/><p id='footer'>\n"
            "Powered by <a href='http://woboq.com'><img alt='Woboq' src='http://code.woboq.org/woboq-16.png' width='41' height='16' /></a> <a href='http://code.woboq.org'>Code Browser</a> "
            CODEBROWSER_VERSION "\n</p>\n</body></html>\n";
}

int main(int argc, char **argv) {

    std::string root;
    bool skipOptions = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!skipOptions && arg[0]=='-') {
            if (arg == "--") {
                skipOptions = true;
            } else {
                //.....
            }
        } else {
            if (root.empty()) {
                root = arg;
            } else {
                root = "";
                break;
            }
        }
    }

    if (root.empty()) {
        std::cerr << "Usage: " << argv[0] << " <path> -d <desc_html>" << std::endl;
        return -1;
    }
    std::ifstream fileIndex(root + "/" + "fileIndex");
    std::string line;

    FolderInfo rootInfo;
    while (std::getline(fileIndex, line))
    {
        FolderInfo *parent = &rootInfo;
        std::vector<std::string> parts;
        boost::split(parts, line, boost::is_any_of("/"));
        auto it = parts.cbegin();
        if (it == parts.cend())
            continue;
        auto end = --parts.cend();
        while(it != end) {
            auto &sub = parent->subfolders[*it];
            if (!sub) sub = std::make_shared<FolderInfo>();
            parent = sub.get();
            ++it;
        }
        parent->subfolders[*it]; //make sure it exists;
    }
    gererateRecursisively(&rootInfo, root, "");
    return 0;
}
