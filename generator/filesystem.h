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

#pragma once

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>

#include <system_error>

namespace llvm {
class Twine;
}

/* Is declared in llvm::sys::fs,  but not implemented */
std::error_code canonicalize(const llvm::Twine &path, llvm::SmallVectorImpl<char> &result);

/* The one in llvm::sys::fs do not create the directory with the right peromissions */
std::error_code create_directories(const llvm::Twine &path);

std::string naive_uncomplete(llvm::StringRef base, llvm::StringRef path);

void make_forward_slashes(char *str);
void make_forward_slashes(std::string &str);
void replace_invalid_filename_chars(std::string &str);
