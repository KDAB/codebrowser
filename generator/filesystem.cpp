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

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/PathV2.h>

#include <unistd.h>
#include <sys/stat.h>

#include <iostream>


llvm::error_code canonicalize(const llvm::Twine &path, llvm::SmallVectorImpl<char> &result) {
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
    return llvm::error_code::success();
}

/* Based on the one from Support/Unix/PathV2 but with different default rights */
static llvm::error_code create_directory(const llvm::Twine& path)
{
    using namespace llvm;
    SmallString<128> path_storage;
    StringRef p = path.toNullTerminatedStringRef(path_storage);
    if (::mkdir(p.begin(), 0755) == -1) {
        if (errno != errc::file_exists)
            return error_code(errno, system_category());
    }
    return error_code::success();
}

llvm::error_code create_directories(const llvm::Twine& path)
{
    using namespace llvm;
    using namespace llvm::sys;
    SmallString<128> path_storage;
    StringRef p = path.toStringRef(path_storage);
    StringRef parent = path::parent_path(p);
    if (!parent.empty()) {
        bool parent_exists;
        if (error_code ec = fs::exists(parent, parent_exists)) return ec;

        if (!parent_exists)
            if (error_code ec = create_directories(parent)) return ec;
    }

    return create_directory(p);
}

