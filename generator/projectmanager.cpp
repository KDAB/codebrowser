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

#include "projectmanager.h"
#include "filesystem.h"
#include "stringbuilder.h"
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>

void ProjectManager::addProject(ProjectInfo info) {
    if (info.source_path.empty())
        return;
    llvm::SmallString<256> filename;
    canonicalize(info.source_path, filename);
    if (filename[filename.size()-1] != '/')
        filename += '/';
    info.source_path = filename.c_str();

    projects.push_back( std::move(info) );
}

ProjectInfo* ProjectManager::projectForFile(llvm::StringRef filename)
{
    unsigned int match_length = 0;
    ProjectInfo *result = nullptr;

    for (auto &it : projects) {
        const std::string &source_path = it.source_path;
        if (source_path.size() < match_length)
            continue;
        if (filename.startswith(source_path)) {
            result = &it;
            match_length = source_path.size();
        }
    }
    return result;
}

bool ProjectManager::shouldProcess(llvm::StringRef filename, ProjectInfo* project)
{
    if (!project)
        return false;
    if (project->type == ProjectInfo::External)
        return false;

    std::string fn = outputPrefix % "/" % project->name % "/" % filename.substr(project->source_path.size()) % ".html";
    return !llvm::sys::fs::exists(fn);
            // || boost::filesystem::last_write_time(p) < entry->getModificationTime();
}

