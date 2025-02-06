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


#include <algorithm>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "../global.h"

const char *data_url = "../data";

std::map<std::string, std::string, std::greater<std::string> > project_map;

struct FolderInfo {
//    std::string name;
    std::map<std::string, std::shared_ptr<FolderInfo>> subfolders;
};

std::string extractMetaFromHTML(std::string metaName, std::string fullPath) {
    std::ifstream filein(fullPath, std::ifstream::in);
    std::string needle = "<meta name=\"woboq:interestingDefinitions\" content=\"";
    std::string endneedle = "\"/>\n";
    for (std::string line; std::getline(filein, line); ) {
        if (line.find(needle, 0) == 0) {
            return line.substr(needle.length(), line.length() - needle.length() - endneedle.length());
        }
    }
    return "";
}

std::string cutNameSpace(std::string &className) {
    int colonPos = className.find_last_of("::");
    if (colonPos != std::string::npos)
        return className.substr(colonPos+1);
    else
        return className;
}

void linkInterestingDefinitions(std::ofstream &myfile, std::string linkFile, std::string &interestingDefitions)
{
    if (interestingDefitions.length() == 0) {
        return;
    }
    myfile << "<ul>";
    std::istringstream f(interestingDefitions);
    std::string className;
    while (std::getline(f, className, ',')) {
        if (className.length() == 0) {
            continue;
        }
        myfile << "<li><a href='" << linkFile << "#" << className << "'"
               << " title='" << className << "'" << ">";
        if (className.find("(anonymous") == std::string::npos) {
            className = cutNameSpace(className);
        }
        myfile << className  << "</a></li>";
    }
    myfile << "</ul>";

}

void gererateRecursisively(FolderInfo *folder, const std::string &root, const std::string &path, const std::string &rel = "") {
    std::ofstream myfile;
    std::string filename = root + "/" + path + "index.html";
    myfile.open(filename);
    if (!myfile) {
        std::cerr << "Error generating " << filename << std::endl;
        return;
    }
    std::cerr << "Generating " << filename << std::endl;

    std::string data_path = data_url[0] == '.' ? (rel + data_url) : std::string(data_url);


    size_t pos = root.rfind('/', root.size()-2);
    std::string project = pos < root.size() ? root.substr(pos+1) : root;
    std::string breadcrumb = "<a href=''>" +path + "</a>";
    std::string parent;

    pos = path.rfind('/', path.size()-2);
    if (pos < path.size()) {
      breadcrumb = "<a href=''>" + path.substr(pos+1)+ "</a>";

      unsigned int next_pos;
      while (pos > 0 && (next_pos = path.rfind('/', pos-1)) < path.size()) {
          if (pos != next_pos +1) {
              parent += "../";
              breadcrumb = "<a href='" +parent +"'>" + path.substr(next_pos + 1, pos - next_pos - 1) + "</a>/" + breadcrumb;
          }
          pos = next_pos;
      }
      if (pos > 1) {
          parent += "../";
          breadcrumb = "<a href='" +parent +"'>" + path.substr(0, pos) + "</a>/" + breadcrumb;
      }
    }
    if (path.length() > 0) {
        breadcrumb = "<a href='../" +parent +"'>" + project + "</a>/" + breadcrumb;
    } else {
        breadcrumb = "<a href=''>" + project + "</a>/" + breadcrumb;
    }

    myfile << "<!doctype html>\n"
              "<head><title>";
    myfile << project << "/" << path;
    myfile << " Source Tree - Woboq Code Browser</title>\n"
           << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
              "<link rel=\"stylesheet\" href=\"" << data_path << "/indexstyle.css\"/>\n";
    myfile << "<script type=\"text/javascript\" src=\"" << data_path << "/jquery/jquery.min.js\"></script>\n";
    myfile << "<script type=\"text/javascript\" src=\"" << data_path << "/jquery/jquery-ui.min.js\"></script>\n";
    myfile << "<script>var path = '"<< path <<"'; var root_path = '"<< rel <<"'; var project='"<< project <<"'; var ecma_script_api_version = 2;</script>\n"
              "<script src='" << data_path << "/indexscript.js'></script>\n"
              "</head>\n<body>\n";
    myfile << "<div id='header'><div id='toprightlogo'><a href='https://code.woboq.org'></a></div>\n";
    myfile << "<p><input id='searchline' placeholder='Search for a file or function'  type='text'/></p>\n";
    myfile << "<h1 id='title'>Browse the source code of " << breadcrumb << " online</h1>\n";
    myfile << "</div><hr/><table id='tree'>\n";

    //if (!path.empty())
    {
        myfile << " <tr><td class='parent'>    <a href='../'>../</a></td><td></td></tr>\n";
    }

    for (auto it : folder->subfolders) {
        const std::string &name = it.first;
        if (it.second) {
            gererateRecursisively(it.second.get(), root, path+name+"/", rel + "../");
            myfile << "<tr><td class='folder'><a href='"<< name <<"/' class='opener' data-path='" << path << name << "'>[+]</a> "
                      "<a href='" << name << "/'>" << name << "/</a></td><td></td></tr>\n";
        } else {
            std::string interestingDefintions = extractMetaFromHTML("woboq:interestingDefinitions", root + "/" + path + name + ".html");
            myfile << "<tr><td class='file'>    <a href='" << name << ".html'>"
                   << name
                   << "</a>"
                   << "<span class='meta'>";
            linkInterestingDefinitions(myfile, name+".html", interestingDefintions);
            myfile << "</span>"
                   << "</td>"
                   << "</tr>\n";
        }
    }

    char timebuf[80];
    auto now = std::time(0);
    auto tm = std::localtime(&now);
    std::strftime(timebuf, sizeof(timebuf), "%Y-%b-%d", tm);

    myfile << "</table>"
            "<hr/><p id='footer'>\n"
            "Generated on <em>" << timebuf << "</em>";

    auto it = project_map.lower_bound(path);
    if (it != project_map.end() && std::equal(it->first.begin(), it->first.end(), path.c_str())) {
        myfile << " from project " << it->first;
        if (!it->second.empty()) {
            myfile <<" revision <em>" << it->second << "</em>";
        }
    }
    myfile << "<br />Powered by <a href='https://woboq.com'><img alt='Woboq' src='https://code.woboq.org/woboq-16.png' width='41' height='16' /></a> <a href='https://code.woboq.org'>Code Browser</a> "
            CODEBROWSER_VERSION "\n<br/>Generator usage only permitted with license</p>\n</body></html>\n";
}

int main(int argc, char **argv) {

    std::string root;
    bool skipOptions = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!skipOptions && arg[0]=='-') {
            if (arg == "--") {
                skipOptions = true;
            } else if (arg=="-d") {
                i++;
                if (i < argc)
                  data_url = argv[i];
            } else if (arg=="-p") {
                i++;
                if (i < argc) {
                    std::string s = argv[i];
                    auto colonPos = s.find(':');
                    if (colonPos >= s.size()) {
                        std::cerr << "fail to parse project option : " << s << std::endl;
                        continue;
                    }
                    auto secondColonPos = s.find(':', colonPos+1);
                    if (secondColonPos < s.size()) {
                        project_map[s.substr(0, colonPos)] = s.substr(secondColonPos + 1);
                    }
                }
            } else if (arg=="-e") {
                i++;
                // ignore -e XXX  for compatibility with the generator project definitions
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
        std::cerr << "Usage: " << argv[0] << " <path> [-d data_url] [-p project_definition]" << std::endl;
        return -1;
    }
    std::ifstream fileIndex(root + "/" + "fileIndex");
    std::string line;

    FolderInfo rootInfo;
    while (std::getline(fileIndex, line))
    {
        FolderInfo *parent = &rootInfo;

        unsigned int pos = 0;
        unsigned int next_pos;
        while ((next_pos = line.find('/', pos)) < line.size()) {
            auto &sub = parent->subfolders[line.substr(pos, next_pos - pos)];
            if (!sub) sub = std::make_shared<FolderInfo>();
            parent = sub.get();
            pos = next_pos + 1;
        }
        parent->subfolders[line.substr(pos)]; //make sure it exists;
    }
    gererateRecursisively(&rootInfo, root, "");
    return 0;
}
