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

#include "commenthandler.h"
#include "generator.h"
#include "stringbuilder.h"

void CommentHandler::handleComment(Generator& generator, const char *bufferStart,
                                   int commentStart, int len,
                                   clang::SourceLocation searchLocBegin, clang::SourceLocation searchLocEnd)
{
    std::string attributes;

    // Try to find a matching declaration
    const auto &dof = decl_offsets;
    //is there one and one single decl in that range.
    auto it_before = dof.lower_bound(searchLocBegin);
    auto it_after = dof.upper_bound(searchLocEnd);
    if (it_before != dof.end() && it_before == (--it_after)) {
        if (it_before->second.second) {
            docs.insert({it_before->second.first, std::string(bufferStart + commentStart, bufferStart + commentStart + len)});
        } else {
            attributes = "data-doc=\"" % it_before->second.first % "\"";
        }
    }


    generator.addTag("i", attributes, commentStart, len);
}
