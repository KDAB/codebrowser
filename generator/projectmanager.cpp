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

#include "projectmanager.h"
#include "filesystem.h"
#include "stringbuilder.h"
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <clang/Basic/Version.h>

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

std::string ProjectManager::includeRecovery(llvm::StringRef includeName, llvm::StringRef from)
{
#if CLANG_VERSION_MAJOR != 3 || CLANG_VERSION_MINOR >= 5
    if (includeRecoveryCache.empty()) {
        for (const auto &proj : projects) {
            // skip sub project
            llvm::StringRef sourcePath(proj.source_path);
            auto parentPath = sourcePath.substr(0, sourcePath.rfind('/'));
            if (projectForFile(parentPath))
                continue;

            std::error_code EC;
            for (llvm::sys::fs::recursive_directory_iterator it(sourcePath, EC), DirEnd;
                    it != DirEnd && !EC; it.increment(EC)) {
                auto fileName = llvm::sys::path::filename(it->path());
                if (fileName.startswith(".")) {
                    it.no_push();
                    continue;
                }
                includeRecoveryCache.insert({std::string(fileName), it->path()});
            }
        }
    }
    llvm::StringRef includeFileName = llvm::sys::path::filename(includeName);
    std::string resolved;
    int weight = -1000;
    auto range = includeRecoveryCache.equal_range(includeFileName);
    for (auto it = range.first; it != range.second; ++it) {
        llvm::StringRef candidate(it->second);
        uint suf_len = 0;
        while (suf_len < std::min(candidate.size(), includeName.size())) {
            if(candidate[candidate.size()-suf_len-1] != includeName[candidate.size()-suf_len-1])
                break;
            suf_len++;
        }
        //Each paths part that are similar from the expected name are weighted 1000 points f
        int w = includeName.substr(candidate.size()-suf_len).count('/') * 1000;
        if (w + 1000 < weight)
            continue;

        // after that, order by similarity with the from url
        uint pref_len = 0;
        while (pref_len < std::min(candidate.size(), from.size())) {
            if(candidate[pref_len] != from[pref_len])
                break;
            pref_len++;
        }
        w += candidate.substr(0, pref_len).count('/') * 10;

        // and the smaller the path, the better
        w -= candidate.count('/');

        if (w < weight)
            continue;

        weight = w;
        resolved = candidate;
    }
    return resolved;
#else
    return {}; // Not supported with clang < 3.4
#endif
}
