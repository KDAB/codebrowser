/****************************************************************************
 * Copyright (C) 2013 Woboq UG (haftungsbeschraenkt)
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

#include "filesystem.h"

#include <clang/Basic/Version.h>
#include <llvm/ADT/Twine.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

#include <iostream>

#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#else
#include <windows.h> // MAX_PATH
#endif

void make_forward_slashes(char *str)
{
    char *c = strchr(str, '\\');
    while (c) {
        *c = '/';
        c = strchr(c + 1, '\\');
    }
}
void make_forward_slashes(std::string &str)
{
    std::replace(str.begin(), str.end(), '\\', '/');
}

// ATTENTION: Keep in sync with ECMAScript function of the same name in .js files and `escapeAttrForFilename` generator.cpp
void replace_invalid_filename_chars(std::string &str)
{
    std::replace(str.begin(), str.end(), ':', '.');
}

std::error_code canonicalize(const llvm::Twine &path, llvm::SmallVectorImpl<char> &result) {
    std::string p = path.str();
#if CLANG_VERSION_MAJOR>=5
    llvm::sys::fs::real_path(path, result);
#else
#ifdef PATH_MAX
    int path_max = PATH_MAX;
#elif defined(MAX_PATH)
    unsigned int path_max = MAX_PATH;
#else
    int path_max = pathconf(p.c_str(), _PC_PATH_MAX);
    if (path_max <= 0)
        path_max = 4096;
#endif

    result.resize(path_max);
    realpath(p.c_str(), result.data());

    result.resize(strlen(result.data()));
#endif

#ifdef _WIN32
    // Make sure we use forward slashes to make sure folder detection works as expected everywhere
    make_forward_slashes(result.data());
#endif

    return {};
}

#if (CLANG_VERSION_MAJOR>=3 && CLANG_VERSION_MINOR>=8) || CLANG_VERSION_MAJOR>3
std::error_code create_directories(const llvm::Twine& path)
{
    using namespace llvm::sys::fs;
    auto defaultPerms = perms::all_all
            & ~perms::group_write
            & ~perms::others_write;

    return llvm::sys::fs::create_directories(path, true, defaultPerms);
}
#else
/* Based on the one from Support/Unix/PathV2 but with different default rights */
static std::error_code create_directory(const llvm::Twine& path)
{
    using namespace llvm;
    SmallString<128> path_storage;
    StringRef p = path.toNullTerminatedStringRef(path_storage);

    if (::mkdir(p.begin(), 0755) == -1) {
        if (errno != static_cast<int>(std::errc::file_exists))
            return {errno, std::system_category()};
    }
    return {};
}

std::error_code create_directories(const llvm::Twine& path)
{
    using namespace llvm;
    using namespace llvm::sys;
    SmallString<128> path_storage;
    StringRef p = path.toStringRef(path_storage);
    StringRef parent = path::parent_path(p);
    if (!parent.empty()) {
        bool parent_exists;
#if CLANG_VERSION_MAJOR==3 && CLANG_VERSION_MINOR<=5
        if (auto ec = fs::exists(parent, parent_exists)) {
#if CLANG_VERSION_MAJOR==3 && CLANG_VERSION_MINOR<=4
            return std::error_code(ec.value(), std::system_category());
#else
            return ec;
#endif
        }
#else
        parent_exists = fs::exists(parent);
#endif

        if (!parent_exists)
            if (auto ec = create_directories(parent)) return ec;
    }

    return create_directory(p);
}
#endif

/**
 * https://svn.boost.org/trac/boost/ticket/1976#comment:2
 *
 * "The idea: uncomplete(/foo/new, /foo/bar) => ../new
 *  The use case for this is any time you get a full path (from an open dialog, perhaps)
 *  and want to store a relative path so that the group of files can be moved to a different
 *  directory without breaking the paths. An IDE would be a simple example, so that the
 *  project file could be safely checked out of subversion."
 *
 * ALGORITHM:
 *  iterate path and base
 * compare all elements so far of path and base
 * whilst they are the same, no write to output
 * when they change, or one runs out:
 *   write to output, ../ times the number of remaining elements in base
 *   write to output, the remaining elements in path
 */
std::string naive_uncomplete(llvm::StringRef base, llvm::StringRef path) {
    using namespace llvm;
    if (sys::path::has_root_path(path)){
        if (sys::path::root_path(path) != sys::path::root_path(base)) {
            return std::string(path);
        } else {
            return naive_uncomplete(sys::path::relative_path(base), sys::path::relative_path(path));
        }
    } else {
        if (sys::path::has_root_path(base)) {
            std::cerr << "naive_uncomplete(" << base.str() << "," << path.str()
                      << "): cannot uncomplete a path relative path from a rooted base" << std::endl;
            return std::string(path);
        } else {
            auto path_it = sys::path::begin(path);
            auto path_it_end = sys::path::end(path);
            auto base_it = sys::path::begin(base);
            auto base_it_end = sys::path::end(base);
            while ( path_it != path_it_end && base_it != base_it_end ) {
                if (*path_it != *base_it) break;
                ++path_it; ++base_it;
            }
            llvm::SmallString<128>  result;
            for (; base_it != base_it_end; ++base_it) {
                sys::path::append(result, "..");
            }
            sys::path::append(result, path_it, path_it_end);
            return std::string(result.str());
        }
    }
}
