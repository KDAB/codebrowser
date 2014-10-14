/****************************************************************************
 * Copyright (C) 2013 Woboq UG (haftungsbeschraenkt)
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

#include "filesystem.h"

#include <clang/Basic/Version.h>
#include <llvm/ADT/Twine.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>

#if CLANG_VERSION_MAJOR==3 && CLANG_VERSION_MINOR<=3
#include <llvm/Support/PathV2.h>
#else
#include <llvm/Support/Path.h>
#endif

#include <unistd.h>
#include <sys/stat.h>

#include <iostream>


std::error_code canonicalize(const llvm::Twine &path, llvm::SmallVectorImpl<char> &result) {
    std::string p = path.str();
#ifdef PATH_MAX
    int path_max = PATH_MAX;
#else
    int path_max = pathconf(p.c_str(), _PC_PATH_MAX);
    if (path_max <= 0)
        path_max = 4096;
#endif
    result.resize(path_max);
    realpath(p.c_str(), result.data());
    result.resize(strlen(result.data()));
    return {};
}

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
        auto parent_exists = fs::exists(parent);

        if (!parent_exists)
            if (auto ec = create_directories(parent)) return ec;
    }

    return create_directory(p);
}


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
            return path;
        } else {
            return naive_uncomplete(sys::path::relative_path(base), sys::path::relative_path(path));
        }
    } else {
        if (sys::path::has_root_path(base)) {
            std::cerr << "naive_uncomplete(" << base.str() << "," << path.str()
                      << "): cannot uncomplete a path relative path from a rooted base" << std::endl;
            return path;
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
            return result.str();
        }
    }
}
