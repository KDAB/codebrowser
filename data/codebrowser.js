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

if (!data_path) {
    // Previous version of the generator (1.7 and before) did not have data_path defined
    var data_path = root_path + "/../data";
}

//Styles:
var setStyle = "";
document.write("<link rel='alternate stylesheet' title='Solarized' href='" + data_path + "/solarized.css' />");
function switchStylestyle(styleName) {
    setStyle = styleName;
    $('link[rel*=style][title]').each(function(i) {
        this.disabled = true;
        if (this.getAttribute('title').toLowerCase() == styleName.toLowerCase()) {
            this.disabled = false;
        }
    });
}
function switchStylestyleAndSetCookie(styleName)
{
    switchStylestyle(styleName);
    createCookie('style', styleName, 5)
}
function createCookie(name,value,days) {
    if (days) {
        var date = new Date();
        date.setTime(date.getTime()+(days*24*60*60*1000));
        var expires = "; expires="+date.toGMTString();
    }
    else var expires = "";
    document.cookie = name+"="+value+expires+"; path=/";
}
function readCookie(name) {
    var nameEQ = name + "=";
    var ca = document.cookie.split(';');
    for(var i=0;i < ca.length;i++) {
        var c = ca[i];
        while (c.charAt(0)==' ') c = c.substring(1,c.length);
        if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
    }
    return null;
}

var style_match = location.search.match(/.*[?&]style=([^#&]+).*/);
if (style_match) {
    var style = style_match[1];
    switchStylestyleAndSetCookie(style);
} else {
    var c = readCookie('style') || "qtcreator";
    if (c) switchStylestyle(c);
}

if (setStyle != "") {
    //Webkit bug  https://bugs.webkit.org/show_bug.cgi?id=115347
    document.write('<style>.code td {white-space: pre }</style>');
}

//-----------------------------------------------------------------------------------

$(function () {
    var start = new Date().getTime();
    var elapsed;

    // ATTENTION: Keep in sync with C++ function of the same name in filesystem.cpp and `Generator::escapeAttrForFilename`
    var replace_invalid_filename_chars = function (str) {
        if(window.ecma_script_api_version && window.ecma_script_api_version >= 2) {
            return str.replace(new RegExp(':', 'g'), '.');
        }

        return str;
    }

    var escape_selector = function (str) {
        return str.replace(/([ #;&,.+*~\':"!^$[\]()=<>|\/@{}\\])/g,'\\$1')
    }

    function escape_html(str) {
        return $("<p/>").text(str).html();
    }

    var useExplain = {
        r: "r: The variable is read",
        w: "w: The variable is modified",
        a: "a: The address is taken",
        c: "c: The function is called",
        m: "m: a member is accessed",
        "?": "?: The type of use of the variable is unknown"
    };

    // demangle the function name, don't care about the template or the argument
    function demangleFunctionName(mangle) {
        if (! mangle) return mangle;
        if (mangle[0] !== '_') return mangle;
        if (mangle[1] === 'M' && mangle[2] === '/') return mangle.slice(3);
        if (mangle[1] !== 'Z') return mangle;
        mangle = mangle.slice(2);
        var result;
        var last = "";
        var scoped = false;
        do {
            if (!result)
                result = "";
            else
                result += "::";
            if (mangle[0]==='D') {
                result += "~" + last;
                break;
            }
            if (mangle[0]==='C') {
                result += last;
                break;
            }
            if (mangle[0]==='N') {
                mangle = mangle.slice(1);
                scoped = true;
            }
            if (mangle[0]==='K') mangle = mangle.slice(1);
            if (mangle[0]==='L') mangle = mangle.slice(1);
            if (mangle.match(/^St/)) { //St
                mangle = mangle.slice(2);
                result += "std::";
            }
            if (mangle[0]==='I') {
                var n = 1;
                var i;
                for (i = 1; i < mangle.length && n > 0 ;i++) {
                    if (mangle[i] === 'I') n++;
                    if (mangle[i] === 'E') n--;
                }
                mangle = mangle.slice(i);
            }
            if (mangle.match(/^[a-z]/)) {
                result += "operator";
                break;
            }
            var len = parseInt(mangle);
            if (!len) return null;
            var start = ("" + len).length;
            last = mangle.substr(start, len);
            result += last;
            mangle = mangle.slice(start + len)
        } while(mangle && mangle[0]!='E' && mangle[0]!='B' && scoped);
        return result;
    }

    // ident and highlight code (for macros)
    function identAndHighlightMacro(origin) {

        // count the number of slashes before character i in origin
        function countSlashes(i) {
            var count = 0;
            while(count < i && origin.charAt(i-count-1) == '\\')
                count++;
            return count;
        }

        var len = origin.length;
        var result = "";
        var ident= "\n";
        var parenLevel = 0;
        var string = 0;  //0 = none  1="  2='
        var lineLen = 0;

        for (var i = 0; i < len; ++i) {
            var c = origin.charAt(i);
            switch (c) {
                case '>': lineLen++; result+="&gt;"; break;
                case '<': lineLen++; result+="&lt;"; break;
                case '&': lineLen++; result+="&amp;"; break;
                case ')':
                    result+=")";
                    if (!string) {
                        parenLevel--;
                        if (parenLevel < 0) parenLevel = 0;
                    }
                    break;
                case '(':
                    result+="(";
                    if (!string && i > 1)
                        parenLevel++;
                    break;
                case ';':
                    result+=";";
                    if (parenLevel==0 && !string) {
                        result += ident;
                        lineLen = 0;
                    }
                    break;
                case '{':
                    result+="{";
                    if (parenLevel==0 && !string) {
                        ident+="  ";
                        result+=ident;
                        lineLen = 0;
                    }
                    break;
                case '}':
                    if (parenLevel==0 && !string) {
                        if (lineLen == 0 && ident.length > 2)
                            result = result.slice(0, -2)
                        result += "}";
                        ident = ident.slice(0, -2);
                        if (i+1 < len && origin.charAt(i+1) != ';') {
                            result += ident;
                            lineLen = 0;
                        }
                    } else {
                        result+="}";
                    }
                    break;
                case '"':
                    if (string == 0) {
                        result += "<q>\""
                        string = 1;
                    } else if (string == 1 && (countSlashes(i)%2) == 0) {
                        string = 0;
                        result += "\"</q>"
                    } else {
                        result += c;
                    }
                    break;
                case '\'':
                    if (string == 0) {
                        result += "<kbd>\'"
                        string = 2;
                    } else if (string == 2 && (countSlashes(i)%2) == 0) {
                        string = 0;
                        result += "\'</kbd>"
                    } else {
                        result += c;
                    }
                    break;
                case ' ':
                    if (lineLen != 0)
                        result += " ";
                    break;
                default:
                    lineLen++;
                    result+=c;
                    break;
            }
        }
        result = result.replace(/\b(auto|void|int|bool|long|uint|unsigned|signed|char|float|double|volatile|const)\b/g,"<em>$1</em>");
        result = result.replace(/\b(asm|__attribute__|break|case|catch|class|__finally|__exception|__try|const_cast|continue|copnstexpr|private|public|protected|__declspec|default|delete|deprecated|dllexport|dllimport|do|dynamic_cast|else|enum|explicit|extern|if|for|friend|goto|inline|mutable|naked|namespace|new|noinline|noreturn|nothrow|operator|register|reinterpret_cast|return|selectany|sizeof|static|static_cast|struct|switch|template|this|thread|throw|true|typeof|false|try|typedef|typeid|typename|union|using|uuid|virtual|while)\b/g,"<b>$1</b>");
        result = result.replace(/\b(\d+)\b/g,"<var>$1</var>");
        result = result.replace(/\b(0x[0-9a-f]+)\b/gi,"<var>$1</var>");
        return result;
    }

    //compute the length of the common prefix between two strings
    // duplicated indexscript.js
    var prefixLen = function( s1 , s2) {
        var maxMatchLen = Math.min(s1.length, s2.length);
        var res = -1;
        while (++res < maxMatchLen) {
            if (s1.charAt(res) != s2.charAt(res))
                break;
        }
        return res * 256 + 256 - s1.length;
    }

    function absoluteUrl(relative) {
        var a = document.createElement('a');
        a.href = relative;
        return a.href;
    }

    function computeRelativeUrlTo(source, dest) {
        var src_splitted = source.split("/");
        if (src_splitted.length > 0 && src_splitted[src_splitted.length-1] == "")
            src_splitted.pop();
        var dst_splitted = dest.split("/");
        var maxMatch = Math.min(src_splitted.length, dst_splitted.length);
        var pre = 0;
        while (++pre < maxMatch) {
            if (src_splitted[pre] != dst_splitted[pre])
                break;
        }
        // make sure the host is the same (http://xxx/ is 3 parts)
        if (pre < 3)
            return dest;

        var stack = [];
        for (i = 0; i < src_splitted.length - pre; ++i) {
            stack.push("..");
        }
        return stack.concat(dst_splitted.slice(pre)).join('/');
    }
    /*// Test
    function test_cmp(a, b) { if (a!=b) { console.log("ASSERT", a, b); alert("FAIL! \n" + a + " != " + b); } }
    test_cmp(computeRelativeUrlTo("", ""), "");
    test_cmp(computeRelativeUrlTo("http://localhost/abcd", "http://code.woboq.org/abcd/e"), "http://code.woboq.org/abcd/e");
    test_cmp(computeRelativeUrlTo("http://localhost/abcd", "http://localhost/abcd/e"), "e");
    test_cmp(computeRelativeUrlTo("http://localhost/abcd/e", "http://localhost/abcd/"), "../");
    test_cmp(computeRelativeUrlTo("http://localhost/abcd/e/", "http://localhost/abcd/"), "../");
    test_cmp(computeRelativeUrlTo("http://localhost/abcd/e/", "http://localhost/abcd/foo"), "../foo");
    test_cmp(computeRelativeUrlTo("http://localhost/abcd/e/f", "http://localhost/abcd/f/foo"), "../../f/foo");
    test_cmp(computeRelativeUrlTo("http://code.woboq.org/qt5/", "http://code.woboq.org/qt5/hello"), "hello");
    */


    var tooltip = {
        ref: "", //the 'ref' of the current symbol displayed
        showTimerId : null,
        hideTimerId : null,
        tooltip : {}, // set when the document is initialized
        showDelay : 350,
        normalHideDelay : 200, // time to hide the tooltip if the cursor was not on it
        focusHideDelay: 700, // time to hide the tooltip after when it was hovered
        hideDelay : this.normalHideDelay,
        gap : 12,
        elem : null,

        init: function() {
            $("div#content").append("<div id='tooltip' style='position:absolute' />");
            this.tooltip = $("#tooltip");
            var this_ = this;
            this.tooltip.hover(
                function () {
                    this_.hideDelay = this_.focusHideDelay;
                    clearTimeout(this_.hideTimerId);
                },
                function () { this_.hideAfterDelay(); }
            );
        },

        setUnderElem: function(elem) {
            var content=$("div#content")
            var docwidth = content.innerWidth()-15;
            var contentTop = $("#content").offset().top;
            var winheight= $(window).height() - 18 - contentTop;
            var toppos = window.scrollY + contentTop;
            var twidth=this.tooltip.get(0).offsetWidth;
            var theight=this.tooltip.get(0).offsetHeight;
            var tipx=elem.offset().left + elem.width()/2 - twidth/2 ;
            tipx += content.scrollLeft();
            if (tipx+twidth>docwidth) tipx = docwidth - twidth - this.gap;
            else if (tipx < 0) tipx = this.gap;
            var tipy=elem.offset().top + elem.height()/2 + this.gap;
            tipy += content.scrollTop();
            tipy=(tipy-toppos+theight>winheight && tipy-theight>toppos) ? tipy-theight-(2*this.gap) : tipy //account for bottom edge
            this.elem = elem;
            this.tooltip.css({left: tipx, top: tipy});
        },

        showAfterDelay: function(elem, additionalFunction) {
            //this.tooltip.hide();
            clearTimeout(this.showTimerId)
            var tt = this;
            this.showTimerId = setTimeout( function() {
                clearTimeout(tt.hideTimerId);
                if (additionalFunction)
                    additionalFunction();
                tt.tooltip.stop(true, true);
                tt.tooltip.fadeIn();
                tt.setUnderElem(elem);
                tt.hideDelay = tt.normalHideDelay;
            }, this.showDelay);
        },

        hideAfterDelay: function(e) {
            clearTimeout(this.showTimerId);
            clearTimeout(this.hideTimerId);
            var tooltip = this.tooltip;
            this.hideTimerId = setTimeout( function() {
                tooltip.stop(true, true);
                tooltip.fadeOut();
            }, this.hideDelay);
        },

        setHtml: function(html) {
            this.tooltip.html(html)
        }

    };

    tooltip.init();

/*-------------------------------------------------------------------------------------*/

    //highlight the line numbers of the warnings
    $(".warning, .error").each(function() {
        var t = $(this);
        var l = t.parents("tr").find("th");
        l.css( { "border-radius": 3, "background-color": t.css("border-bottom-color") });
        l.attr("title", t.attr("title"));
    } );

    // other highlighting stuff
    var highlighted_items;
    var highlight_items = function(ref) {
        if (highlighted_items)
            highlighted_items.removeClass("highlight");
        if (ref) {
            highlighted_items = $("[data-ref='"+escape_selector(ref)+"']");
            highlighted_items.addClass("highlight")
        }
    }

    var anchor_id  = location.hash.substr(1); //Get the word after the hash from the url
    if (/^\d+$/.test(anchor_id)) {
        highlighted_items = $("#" + anchor_id);
        highlighted_items.addClass("highlight")
        scrollToAnchor(anchor_id, false);
    } else if (/^\d+-\d+$/.test(anchor_id)) {
        var m = anchor_id.match(/^(\d+)-(\d+)$/);
        var a = parseInt(m[1]);
        var b = parseInt(m[2]);
        if (a && b && a <= b) {
            var select = "#" + a;
            for (var x = a + 1; x <= b; ++x) {
                select += ",#" + x;
            }
        }
        highlighted_items = $(select);
        highlighted_items.addClass("highlight")
        scrollToAnchor("" + a, false);
    } else if (anchor_id != "") {
        highlight_items(anchor_id);
        scrollToAnchor(anchor_id, false);
    }

/*-------------------------------------------------------------------------------------*/
    var skipHighlightTimerId = null;
    var onMouseLeave = function(e) { tooltip.hideAfterDelay(e); }
    var onMouseClick = function(e) {
        if (e.ctrlKey || e.altKey || e.button != 0) return true; // don't break ctrl+click,  open in a new tab
        if (!this.href) return true; // not clicking on a link
        var toppos;
        if (this.parentNode.tagName == "TD") {
            // The node is part of the code, find out the context from there.
            toppos = $(this).offset().top
        } else if (tooltip.tooltip.is(":visible") && tooltip.elem) {
            // If the tooltip is open, use the item from the tooltip
            toppos = tooltip.elem.offset().top;
        } else {
            // else, from the top.
            var contentTop = $("#content").offset().top;
            toppos = window.scrollY + contentTop;
        }
        var context = undefined;
        $('.def').each(function() {
            var t = $(this);
            if (t.offset().top > toppos + 1) {
                return false;
            }
            context = t;
        });
        if (context !== undefined) {
            if (context.hasClass("decl")) {
                var c = context[0].title_;
                if (c === undefined)
                    c = context.attr("title");
                var ref = context.attr("data-ref");
                pushHistoryLog( { url: location.origin + location.pathname + "#" + ref, name: c, ref: ref} );
            }
        }

        var ref = $(this).attr("data-ref")
        if (ref && ref.match(/^[^0-9].*/)) {
            if (ref.match(/^_M\//)) { // Macro
                var currentLine = $(this).parents("tr").find("th").text();
                pushHistoryLog( { url: location.origin + location.pathname + "#" + currentLine, ref: ref } );
            } else {
                pushHistoryLog( { url: this.href, ref: ref } );
            }
        }

        tooltip.tooltip.hide();
        skipHighlightTimerId = setTimeout(function() { skipHighlightTimerId = null }, 600);

        if (history && history.pushState) {
            var href = this.href;
            var hashPos = href.indexOf("#");
            if (hashPos >= 0) {
                var anchor = href.substr(hashPos+1);
                var url = href.substr(0, hashPos);
                if (url == "" || url === location.origin + location.pathname) {
                    scrollToAnchor(anchor, true)
                    e.preventDefault();
                    return false;
                }
            }
        }

        return true;
    }

    // Mouse interaction (tooltip, ...)
    var onMouseEnterRef = function(e) {
        if (skipHighlightTimerId) return false;
        var elem = $(this);
        var isMacro = elem.hasClass("macro");
        var ref = elem.attr("data-ref");
        var proj = elem.attr("data-proj");

        var proj_root_path = root_path;
        if (proj) { proj_root_path = projects[proj]; }

        var url = proj_root_path + "/refs/" + replace_invalid_filename_chars(ref);

        if (!$(this).hasClass("highlight")) {
            highlight_items(ref);
        }

        var computeTooltipContent = function(data, title, id) {
            var type ="", content ="";
            var tt = tooltip.tooltip;
            var showUseFunc = function(e) {
                e.stopPropagation();
                tt.find(".uses").toggle();
                return false;
            };

            var symbolUrl;
            if (data) {  // this mean the ref symbol exists
                var absoluteRoot = absoluteUrl(proj_root_path);
                var absoluteDataPath = absoluteUrl(data_path);
                symbolUrl = data_path + "/symbol.html?root=" + computeRelativeUrlTo(absoluteDataPath, absoluteRoot) + "&ref=" + ref;
            }

            if (elem.hasClass("local") || elem.hasClass("tu") || elem.hasClass("lbl")
                    || (isMacro && !data && ref)) {
                type = $("#" + escape_selector(ref)).attr("data-type");

                var docs = $("i[data-doc='"+escape_selector(ref)+"']");
                docs.each(function() {
                    var comment = $(this).html();
                    content += "<br/><i>" + comment + "</i>";
                    var l = $(this).parent().prev("th").text();
                    if (l) {
                        var url = "#" + l;
                        content += " <a href='" + url +"'>&#8618;</a>";
                    }

                });

                //var uses = highlighted_items;
                var uses = $(".code [data-ref='"+escape_selector(ref)+"']");
                var usesLis ="";
                var usesCount = 0;
                uses.each(function() {
                    var t = $(this);
                    var l = t.parents("td").prev("th").text();

                    if (t.hasClass("def")) {
                        content += "<br/><a href='#"+ l +"'>Definition</a>";
                    } else if (t.hasClass("decl") || this.nodeName === "DFN") {
                        content += "<br/><a href='#"+ l +"'>Declaration</a>";
                    } else {
                        var c;
                        if (elem.hasClass("tu")) {
                            // Find the context:  Look at up every line from the current one if
                            // there is a .def,  if this definition is a declaration, it is the context
                            var prevLines = t.closest("tr").prevAll();
                            for (var x = 0; x < prevLines.length; ++x) {
                                var context = $(prevLines[x]).find(".def");
                                if (!context.length)
                                    continue;
                                if (context.length == 1 && context.hasClass("decl")) {
                                    c = context[0].title_;
                                    if (c === undefined)
                                        c = context.attr("title")
                                }
                                break;
                            }
                        }
                        if (!c) c = "line " + l;
                        var useType = "";
                        var ut = t.attr("data-use");
                        if (ut) {
                            useType += " (<abbr title='"+ useExplain[ut] +"'>"+escape_html(ut)+"</abbr>)";
                        }
                        usesLis += "<li><a href='#"+ l +"'>"+ escape_html(c) +"</a>"+useType+"</li>"
                        usesCount += 1;
                    }
                });

                if (usesCount > 0)
                    content += "<br/><a href='#' class='showuse'>Show Uses:</a> (" + usesCount + ")<br/><ul class='uses'>" + usesLis + "</ul>"

            } else if (elem.hasClass("typedef")) {
                type = elem.attr("data-type");
            } else {
                var res = $("<data>"+data+"</data>");
                var isType = elem.hasClass("type");

                var typePrefixLen = -1;

                //comments:
                var seen_comments = [];
                res.find("doc").each(function() {
                    var comment = $(this).html();
                    if ($.inArray(comment, seen_comments) !== -1)
                        return;
                    seen_comments.push(comment);
                    if (comment.length > 550) {
                        // FIXME: we should not split in an escape code
                        comment = comment.substr(0, 500) + "<a href='#' class='expandcomment'> [more...]</a><span style='display:none'>" + comment.substr(500) + "</span>";
                    }
                    content += "<br/><i>" + comment + "</i>";
                    var f = $(this).attr("f");
                    var l = $(this).attr("l");
                    if (f && l) {
                        var url = proj_root_path + "/" + f + ".html#" + l;
                        content += " <a href='" + url +"'>&#8618;</a>";
                    }
                });

                var p = function (label, tag) {
                    var d = res.find(tag);
                    if (!d.length)
                        return false;
                    content += "<br/>" + label + ": (" + d.length + ")";
                    if (tag === "inh" && symbolUrl && isType) {
                        content += " &nbsp; [<a href='"+ symbolUrl +"#graph'>Show Graph</a>]";
                    }
                    var shouldCompress = d.length > 15;
                    var dict = { number: 0 };
                    d.each(function() {
                        var th = $(this);
                        var f = th.attr("f");
                        var l = th.attr("l");
                        var t = th.attr("type");
                        if (t) {
                            var prefixL = prefixLen(f, file)
                            if (prefixL >= typePrefixLen) {
                                typePrefixLen = prefixL;
                                type = t;
                            }
                        }
                        if (shouldCompress) {
                            if (!Object.prototype.hasOwnProperty.call(dict, f)) {
                                dict[f] = [];
                                dict.number++;
                            }
                            dict[f].push(l);
                        } else {
                            var url = proj_root_path + "/" + f + ".html#" + l;
                            content += "<br/><a href='" + url +"' >" + f + ":" + l + "</a>";
                            if (tag === "ovr" || tag === "inh") {
                                var c = th.attr("c");
                                if (c)
                                    content += " (" + demangleFunctionName(c) + ")";
                            }
                        }
                    });
                    if (shouldCompress) {
                        if (dict.number > 40) {
                            content += "<br/>(Too many)";
                            return false;
                        }
                        for(var f in dict) {
                            if (!Object.prototype.hasOwnProperty.call(dict,f) || f==="number") continue;
                            var url_begin = proj_root_path + "/" + f + ".html";
                            content += "<br/><a href='" + url_begin + "#" + dict[f][0] +"' >" + f +  "</a>";
                            var len = dict[f].length;
                            if (len > 100 || (f !== file && len >= 5))
                                content += " (" + len + " times)";
                            else {
                                content += ": <a href='" + url_begin + "#" + dict[f][0] +"' >" + dict[f][0] +"</a>";
                                for(var i = 1; i < len; i++) {
                                    content += ", <a href='" + url_begin + "#" + dict[f][i] +"' >" + dict[f][i] +"</a>";
                                }
                            }
                        }
                    }
                    return true;
                }
                p("Definitions", "def");
                p("Declarations", "dec");
                p(isType ? "Inherit" : "Overrides", "inh");
                p(isType ? "Inherited by" : "Overriden by", "ovr");

                // Size:
                var size = res.find("size");
                if (size.length === 1) {
                    content += "<br/>Size: " + escape_html(size.text()) + " bytes";
                }
                var offset = res.find("offset");
                if (offset.length === 1) {
                    content += "<br/>Offset: " + escape_html(offset.text() >> 3) + " bytes";
                }

                // Uses:
                var uses = res.find("use");
                if (uses.length) {
                    var href ="#";
                    if (symbolUrl) {
                        href = symbolUrl+"#uses";
                    }
                    content += "<br/><a href='" + href + "' class='showuse'>Show Uses:</a> (" + uses.length + ")<br/><span class='uses_placeholder'></span>"
                }
                var useShown = false;
                showUseFunc = function(e) {
                    if (useShown) {
                        tt.find(".uses").toggle();
                        return false;
                    }
                    var dict = { };
                    var usesTypeCount = { };
                    uses.each(function() {
                        var t = $(this);
                        var f = t.attr("f");
                        var l = t.attr("l");
                        var c = t.attr("c");
                        var u = t.attr("u");
                        //if (!u) u = "?"
                        var url = proj_root_path + "/" + f + ".html#" + l;
                        if (!Object.prototype.hasOwnProperty.call(dict, f)) {
                            dict[f] = { elem: $("<li/>").append($("<a/>").attr("href", url).text(f)),
                                        contexts: {},  prefixL: prefixLen(file, f), count: 0,
                                        f: f, brk: t.attr("brk")
                            };
                        }
                        c = demangleFunctionName(c)
                        if (!c) c = f + ":" + l;
                        dict[f].count++;
                        usesTypeCount[u||"?"] = (usesTypeCount[u||"?"]||0) + 1;

                        if (!Object.prototype.hasOwnProperty.call(dict[f].contexts, c)) {
                            dict[f].contexts[c] = $("<li/>").append($("<a/>").attr("href", url).text(c));
                            dict[f].contexts[c].count = 1;
                            if (u) {
                                dict[f].contexts[c].usesType = "<abbr title='"+ useExplain[u] +"'>"+u+"</abbr>";
                                dict[f].contexts[c].usesRaw = u;
                            } else {
                                dict[f].contexts[c].usesType = "";
                                dict[f].contexts[c].usesRaw = "?";
                            }
                        } else {
                            dict[f].contexts[c].count++;
                            if (dict[f].contexts[c].usesRaw.indexOf(u||"?") === -1) {
                                if (u)
                                    dict[f].contexts[c].usesType += "<abbr title='"+ useExplain[u] +"'>"+u+"</abbr>";
                                dict[f].contexts[c].usesRaw += (u||"?");
                            }
                        }
                    });
                    var list = [];
                    for (var xx in dict) {
                        if (Object.prototype.hasOwnProperty.call(dict, xx))
                            list.push(dict[xx]);
                    }
                    list.sort(function(a,b){ var dif = b.prefixL - a.prefixL; return dif ? dif : a.brk ? 1 : b.f - a.f });
                    var ul = $("<ul class='uses'/>");
                    for (var i = 0; i < list.length; ++i) {
                        var usestypes = "";
                        var subul = $("<ul/>");
                        for (var xx in list[i].contexts) if (Object.prototype.hasOwnProperty.call(list[i].contexts, xx)) {
                            var context = list[i].contexts[xx];
                            usestypes += context.usesRaw;
                            subul.append(list[i].contexts[xx].append(" (" + context.count+" " + context.usesType + ")")
                                .attr("data-uses", context.usesRaw));
                        }
                        ul.append(list[i].elem.append(" (" + list[i].count+")").attr("data-uses",usestypes).append(subul));
                    }
                    tt.find(".uses_placeholder").append(ul).html();
                    useShown = true;
                    uses = undefined; // free memory
                    return false;
                }
            }

            tt.empty();
            if (!isMacro) {
                var preTitle = "";
                if (elem.hasClass("fake"))
                    preTitle = "Implicit copy or conversion: ";
                if (symbolUrl) {
                    tt.append($("<b />").append(preTitle, $("<a class='link' href='"+ symbolUrl +"' />").text(title)));
                    tt.append("<span style='float:right'><a href='" + symbolUrl +"'>&#x1f517;</a></span>");
                } else if (id && id != "") {
                    tt.append($("<b />").append(preTitle, $("<a class='link' href='#"+ id +"' />").text(title)));
                } else {
                    tt.append($("<b />").text(preTitle + title));
                }
            } else {
                if (title) {
                    tt.append($("<code class='code' style='white-space: pre-wrap' />").html(title));
                    tt.append("<br/>");
                }
            }
            if (type != "") {
                tt.append("<br/>");
                tt.append($("<span class='type' />").text(type));
            }
            tt.append($("<span />").html(content));
            tooltip.ref = ref;
            tt.find(".uses").hide();
            tt.find(".showuse").mouseup(showUseFunc).click(function() { return false; });
            tt.find(".expandcomment").mouseup(function(e) {
                $(this).toggle();
                $(this).next().toggle();
                return false;
            }).click(function() { return false; });
        }

        if (!this.title_) {
            this.title_ = elem.attr("title");
            elem.removeAttr("title");
            if (isMacro && this.title_) {
                this.title_ = identAndHighlightMacro(this.title_);
            }
            if (!this.title_ && elem.hasClass("lbl")) {
                this.title_ = elem.text();
            }

        }

        var tt = this;
        if (ref && !this.tooltip_loaded && !elem.hasClass("local") && !elem.hasClass("tu")
                && !elem.hasClass("typedef") && !elem.hasClass("lbl")) {
            this.tooltip_loaded = true;
            $.get(url, function(data) {
                tt.tooltip_data = data;
                if (tooltip.ref === ref)
                    computeTooltipContent(data, tt.title_, tt.id);

                // attempt to change the href to the definition

                if (isMacro) {
                    //macro always have the right link already.
                    return;
                }
                var res = $("<data>"+data+"</data>");
                var def =  res.find("def");
                if (def.length > 0) {

                    var currentLine = elem.parents("tr").find("th").text();
                    //if there are several definition we take the one closer in the hierarchy.
                    var result = {  len: -2, brk: true };
                    def.each( function() {
                        var cur = { len : -1,
                                    f : $(this).attr("f"),
                                    l : $(this).attr("l"),
                                    brk : $(this).attr("brk") };

                        if (cur.f === file && cur.l === currentLine)
                            return;

                        cur.len = prefixLen(cur.f, file)
                        if (result.brk == cur.brk ? (cur.len > result.len) : result.brk) {
                            result = cur;
                            result.isMarcro = ($(this).attr("macro"));
                        }
                    });

                    if (result.len >= 0) {
                        var url = proj_root_path + "/" + result.f + ".html#" +
                            (result.isMarcro ? result.l : ref );
                        if (elem.attr("href")) {
                            elem.attr("href", url);
                        } else {
                            if (result.f === file) //because there might be several declaration then
                                elem.removeAttr("id");
                            elem.wrap("<a href='"+url+"'/>");
                        }
                    }
                }
            });
        }
        tooltip.showAfterDelay(elem, function() { computeTooltipContent(tt.tooltip_data, tt.title_, tt.id) })

        return false;
    };

    // #if/#else/... tooltip
    var onMouseEnterPPCond = function(e) {
        if (skipHighlightTimerId) return false;
        if (highlighted_items) {
            highlighted_items.removeClass("highlight");
        }
        var elem = $(this);
        var ppcond = elem.attr("data-ppcond");
        highlighted_items = $("[data-ppcond='"+escape_selector(ppcond)+"']");
        highlighted_items.addClass("highlight")
        var ppcondItems = highlighted_items;
        var currentLine = elem.parents("tr").find("th").text();
        function computePPCondTooltipContent() {
            var tt = tooltip.tooltip;
            tt.empty();

            var contents = $("<ul class='ppcond'/>");
            ppcondItems.each(function() {
                var p = $(this).parent();
                var l = p.parents("tr").find("th").text();
                var t = p.text();
                while (t[t.length - 1] === '\\') {
                    p = p.parent().parent().next().find("u");
                    if (p.length !== 1)
                        break;
                    t = t.slice(0, t.length-1) + "\n" + p.text();
                }
                if (currentLine === l) {
                    contents.append($("<li/>").text(t));
                } else {
                    contents.append($("<li/>").append($("<a href='#" + l + "' />").text(t)));
                }
            });
            tt.append(contents);
        }
        tooltip.showAfterDelay(elem, computePPCondTooltipContent);
    }

    var onMouseEnterArg = function(e) {
        $(this).attr("title", "Argument passed by reference");
        tooltip.showAfterDelay($(this), function() {
            tooltip.tooltip.text("Argument passed by reference");
        });
    }

    var elemWithTooltip;
    var isTouchEvent = false;
    function applyTo(func) { return function(e) {
        if (!isTouchEvent || this === elemWithTooltip) {
            return onMouseClick.apply(this, arguments);
        } else {
            isTouchEvent = false;
            elemWithTooltip = this;
            var oldDelay = tooltip.showDelay;
            tooltip.showDelay = 1;
            func.apply(this, arguments);
            tooltip.showDelay = oldDelay;
            e.preventDefault()
            return false;
        }
    }; };
    var code = $(".code");
    code.on({"mouseenter": onMouseEnterRef, "mouseleave": onMouseLeave, "click": applyTo(onMouseEnterRef) },
                  "[data-ref], .macro");
    code.on({"mouseenter": onMouseEnterPPCond, "mouseleave": onMouseLeave, "click": applyTo(onMouseEnterPPCond)},
                  "[data-ppcond]");
    code.on({"mouseenter": onMouseEnterArg, "mouseleave": onMouseLeave, "click": applyTo(onMouseEnterArg) },
                  ".refarg");
    code.on({"click":onMouseClick }, "th a")
    code.click(function() { tooltip.tooltip.hide(); });

    tooltip.tooltip.on({"mouseup": onMouseClick}, "a")

    $("#header").on({"click":onMouseClick }, "a");

    $(document).bind( "touchstart", function() { isTouchEvent = true; } )


    if (typeof(initRef) !== 'undefined') {
        onMouseEnterRef.apply($(initRef)[0]);
    }
    skipHighlightTimerId = setTimeout(function() { skipHighlightTimerId = null }, 600)


/*-------------------------------------------------------------------------------------*/

    //bread crumbs.
    var breadcrumb = $("h1#breadcrumb");
    if (breadcrumb.length == 0) {
        // compatibility with codebrowser 1.7 and before
        var bread = "<p id='breadcrumb'>";
        var paths = file.split('/');
        for (var i = 0; i < paths.length - 1; ++i) {
            bread+="<a href='";
            if (i === paths.length - 2) bread += "./";
            else {
                for (var ff = 2; ff < paths.length - i; ++ff) {
                    bread += "../";
                }
            }
            bread+= "'>" + paths[i] + "</a>/";
        }
        bread += paths[paths.length -1];
        bread += "<br/><span id='breadcrumb_symbol'/></p>";
        $("#header").append(bread);
    } else {
        breadcrumb.append("<br/><span id='breadcrumb_symbol'/>");
    }

/*-------------------------------------------------------------------------------------*/

    // Search Line
    $("#header").prepend("<input id='searchline' type='text' placeholder='Search a file or function'/>");
    var searchline = $("input#searchline");
    var searchTerms;
    searchline.focus(function() {
        if (searchTerms)
            return;
        searchTerms = {}
        var fileIndex = [];
        var functionDict = {};

        // Do a google seatch of the text on the project.
        var text_search = function(text) {
            var location = "" + (window.location);
            var idx = location.indexOf(file);
            if (idx < 0)
                return;
            location = location.substring(0, idx);
            window.location = "http://google.com/search?sitesearch=" + encodeURIComponent(location) + "&q=" + encodeURIComponent(text);
        }

//BEGIN  code duplicated in indexscript.js
        // callback for jqueryui's autocomple activate
        var activate = function(event,ui) {
            var val = ui.item.value;
            var type = searchTerms[val] && searchTerms[val].type;
            if (type == "file") {
                window.location = root_path + '/' +  searchTerms[val].file + ".html";
            } else if (type == "ref") {
                var ref = searchTerms[val].ref;

                var url = root_path + "/refs/" + replace_invalid_filename_chars(ref);
                $.get(url, function(data) {
                    var res = $("<data>"+data+"</data>");
                    var def =  res.find("def");
                    var result = {  len: -1 };
                    def.each( function() {
                        var cur = { len : -1,
                                    f : $(this).attr("f"),
                                    l : $(this).attr("l") }

                        cur.len = prefixLen(cur.f, file)
                        if (cur.len >= result.len) {
                            result = cur;
                            result.isMarcro = ($(this).attr("macro"));
                        }
                    });

                    if (result.len >= 0) {
                        var newloc = root_path + "/" + result.f + ".html#" +
                            (result.isMarcro ? result.l : ref );
                        window.location = newloc;
                    }
                });
            } else {
                text_search(val);
            }
        };

        var getFnNameKey = function (request) {
            if (request.indexOf('/') != -1 || request.indexOf('.') != -1)
                return false;
            var mx = request.match(/::([^:]{2})[^:]*$/);
            if (mx)
                return mx[1].toLowerCase().replace(/[^a-z]/, '_');
            request = request.replace(/^:*/, "");
            if (request.length < 2)
                return false;
            var k = request.substr(0, 2).toLowerCase();
            return k.replace(/[^a-z]/, '_')
        }

        var autocomplete = function(request, response) {
            var term = $.ui.autocomplete.escapeRegex(request.term);
            var rx1 = new RegExp(term, 'i');
            var rx2 = new RegExp("(^|::)"+term.replace(/^:*/, ''), 'i');
            var functionList = [];
            var k = getFnNameKey(request.term)
            if (k && Object.prototype.hasOwnProperty.call(functionDict,k)) {
                functionList = functionDict[k].filter(
                    function(word) { return word.match(rx2) });
            }
            var l = fileIndex.filter( function(word) { return word.match(rx1); });
            l = l.concat(functionList);
            l = l.slice(0,1000); // too big lists are too slow
            response(l);
        };

        searchline.autocomplete( {source: autocomplete, select: activate, minLength: 4  } );

        searchline.keypress(function(e) {
            var value = searchline.val();
            if(e.which == 13) {
                activate({}, { item: { value: value } });
            }
        });

        // When the content changes, fetch the list of function that starts with ...
        searchline.on('input', function() {
            var value = $(this).val();
            var k = getFnNameKey(value);
            if (k && !Object.prototype.hasOwnProperty.call(functionDict, k)) {
                functionDict[k] = []
                $.get(root_path + '/fnSearch/' + k, function(data) {
                    var list = data.split("\n");
                    for (var i = 0; i < list.length; ++i) {
                        var sep = list[i].indexOf('|');
                        var ref = list[i].slice(0, sep);
                        var name = list[i].slice(sep+1);
                        searchTerms[name] = { type:"ref", ref: ref };
                        functionDict[k].push(name);
                    }
                    if (searchline.is(":focus")) {
                        searchline.autocomplete("search", searchline.val());
                    }
                });
            }
        });

        // Pasting should show the autocompletion
        searchline.on("paste", function() { setTimeout(function() {
                searchline.autocomplete("search", searchline.val());
            }, 0);
        });
//END

        // Fetch the list of all files
        $.get(root_path + '/fileIndex', function(data) {
            var list = data.split("\n");
            fileIndex = list;
            for (var i = 0; i < list.length; ++i) {
                searchTerms[list[i]] = { type:"file", file: list[i] };
            }
            if (searchline.is(":focus")) {
                searchline.autocomplete("search", searchline.val());
            }
        });

        return false;
    });

/*-------------------------------------------------------------------------------------*/

    // Find the current context while scrolling
    window.onscroll = function() {
        var contentTop = $("#content").offset().top;
        var toppos = window.scrollY + contentTop;
        var context = undefined;
        $('.def').each(function() {
            var t = $(this);
            if (t.offset().top > toppos + 1) {
                return false;
            }
            context = t;
        });
        var c = "";
        var ref = "";
        if (context !== undefined) {
          if (context.hasClass("decl")) {
              c = context[0].title_;
              if (c === undefined)
                  c = context.attr("title");
              ref = context.attr("id");
          }
        }
        if (ref == "") {
            $("span#breadcrumb_symbol").text(c);
        } else {
            $("span#breadcrumb_symbol").html($("<a class='link' href='#"+ ref +"' />").text(c));
        }
    };

/*-------------------------------------------------------------------------------------*/

    // Options:
    var styleOpt = "";
    $('link[rel*=style][title]').each(function() {
        var title = this.getAttribute('title')
        styleOpt += "<option value='" + title.toLowerCase() +"'";
        if (setStyle.toLowerCase() === title.toLowerCase()) styleOpt += " selected='true'";
        styleOpt += ">" + title + "</option>";
    });
    $("#header").append("<p id='options'><a class='opt_linenum' href='#'>Toggle line number</a> -  Style: <select class='opt_style'>" + styleOpt + "</select></p>")

    var lineNumberShown = -1;
    $(".opt_linenum").click(function() {
        if (lineNumberShown == -1) {
            //add a space to the empty lines so that they keep their height.
            $("td:empty, td i:only-child:empty").append("&nbsp;")
            lineNumberShown = true;
        }
        //toggle is too slow.
        lineNumberShown ? 
            $(".code th").hide() :
            $(".code th").show();
        lineNumberShown = !lineNumberShown;
        return false;
    });
    $(".opt_style").change(function(e) {
        switchStylestyleAndSetCookie(this.options[this.selectedIndex].value);
        //return false;
    });

    var cwo_url = 'https://code.woboq.org';
    $("#header").prepend("<a class='logo' href='" + cwo_url + "'><img src='" + cwo_url + "/data/woboq-48.png'/></a>");

/*-------------------------------------------------------------------------------------*/


    $(".code").on({"mouseenter": function() {
        if (!this.hasLink) {
            this.hasLink = true;
            var t = $(this);
            var def = t.parent().find("dfn[id]");
            if (def && def.length >= 1 && !def.first().hasClass("local")) {
                t.wrapInner("<a href='#"+def.first().attr("id")+"'/>");
            } else {
                t.wrapInner("<a href='#"+t.text()+"'/>");
            }
        }
    }}, "th");

/*-------------------------------------------------------------------------------------*/

    // fix scrolling to an anchor because of the header
    // isLink tells us if we are here because a link was cliked
    function scrollToAnchor(anchor, isLink) {
        var target = $("#" + escape_selector(anchor));
        if (target.length) {
            //Smooth scrolling and let back go to the last location
            var contentTop = $("#content").offset().top;
            if (parseInt(anchor)) {
                // if the anchor is a line number, (or a function local) we want to give a bit more
                // context on top
                contentTop += target.height() * 7; // 7 lines
            }

            if (isLink) {
            //   history.replaceState({contentTop: contentTop, bodyTop: $("body").scrollTop() }, undefined)
                history.pushState({bodyTop: target.offset().top - contentTop},
                                    document.title + "**" + anchor,
                                    window.location.pathname + "#" + anchor);
            }
            //     $("#content").animate({scrollTop:target.position().top + contentTop }, 300);
            $("html,body").animate({scrollTop:target.offset().top - contentTop  }, isLink ? 300 : 1);
        }
    }

    window.onpopstate = function (e) {
        if (!e.state)
            return;
        if (e.state.bodyTop > 0) {
            $("html,body").animate({scrollTop: e.state.bodyTop});
        }
    }

    /*-------------------------------------------------------------------------------------*/

    var isFirefox = typeof InstallTrigger != "undefined";
    if(isFirefox) {
        // Workaround Firefox selection bug with <q>, that would add fake quote in the clip board
        $(".code q").replaceWith(function() { return $("<span class='string'/>").text($(this).text());  });
    }

/*-------------------------------------------------------------------------------------*/

    $('#content').append('<div id="allSideBoxes">');

    // The definitions side bar
    var dfns = document.getElementsByClassName('def');
    if (dfns.length) {
        var dfnsDiv = $('<div id="symbolSideBox" class="sideBox"><h3>Definitions</h3><ul></ul></div>');
        dfnsDiv.find('h3').click(function() {
            var hidden = !$("#symbolSideBox ul").toggle().is(":visible");
            createCookie('symboxhid', hidden, 5);
        });
        dfnsDiv.attr("style", "top:" + document.getElementById('header').clientHeight + "px;");

        var theUl = dfnsDiv.find('ul');
        var html = "";
        for (var i = 0; i < dfns.length - 1; ++i) {
            html += '<li><a href="#' + dfns[i].id + '" title="'+ dfns[i].title+ '" data-ref="'+ dfns[i].id +'">'+escape_html(dfns[i].textContent) +'</a></li>';
        }
        theUl.append(html);

        $('#allSideBoxes').append(dfnsDiv);

        var links = $("#symbolSideBox ul li a");
        links.on({"mouseenter": onMouseEnterRef,
                    "mouseleave": onMouseLeave
                    , "click": applyTo(onMouseEnterRef) });

        if (readCookie('symboxhid') === "true")
            $("#symbolSideBox ul").hide()
    }


    var historylog = [];

    var historyKey = (function() {
        var a = document.createElement('a');
        a.href = root_path;
        return "historylog" + a.pathname;
    })();

    function pushHistoryLog(hist) {
        if (!historylog) historylog = [];
        // don't add if recent history already constains this item
        if (historylog.length >= 1 && historylog[historylog.length - 1].ref === hist.ref) return;
        if (historylog.length >= 2 && historylog[historylog.length - 2].ref === hist.ref) return;
        if (historylog.length >= 3 && historylog[historylog.length - 3].ref === hist.ref) return;
        historylog.push(hist);
        if (historylog.length > 100)
            historylog = historylog.slice(-100);
        if (localStorage)
            localStorage.setItem(historyKey, JSON.stringify(historylog))
        refreshHistoryBox();
    }

    function refreshHistoryBox() {
        try {
            historylog = JSON.parse(localStorage.getItem(historyKey));
            while(typeof historylog === "string") historylog = JSON.parse(historylog);
        } catch(e) {}
        if (historylog && historylog.length >= 1) {
            if ($("#historySideBox").length==0) {
                var dfnsDiv = $('<div id="historySideBox" class="sideBox"><h3>History</h3><ul></ul></div>');
                dfnsDiv.find('h3').click(function() {
                    var hidden = !$("#historySideBox ul").toggle().is(":visible");
                    createCookie('hisboxhid', hidden, 5);
                });
                dfnsDiv.attr("style", "top:" + document.getElementById('header').clientHeight + "px;");

                $('#allSideBoxes').append(dfnsDiv);
                if (readCookie('hisboxhid') === "true")
                    $("#historySideBox ul").hide()
            }

            var html = "";
            historylog.forEach(function(o) {
                var name = o.name;
                if (!name) name = demangleFunctionName(o.ref);
                html = "<li><a href='"+o.url+"' data-ref='"+ o.ref +"' title='"+name+"'>"+escape_html(name)+"</a></li>" + html;
            } );

            var theUl = $('#historySideBox ul');
            theUl.html(html);

            var links = $("#historySideBox ul li a");
            links.on({"mouseenter": onMouseEnterRef,
                        "mouseleave": onMouseLeave
                        , "click": applyTo(onMouseEnterRef) });
        }
    }
    refreshHistoryBox(); // create/load
    if (location.hash && location.hash.length >= 0) {
        // Only do when non-numeric, e.g. if it is a real symbol and not a line number
        var hash = location.hash.replace('#','');
        if (!/^\d+$/.test(hash)) {
            var title = $('#'+escape_selector(hash)).attr('title');
            if (!title || title.length == 0) {
                title = hash;
            }

            pushHistoryLog( { url: location.origin + location.pathname + "#" + hash, name: title, ref: hash} );

        } else {
            // FIXME: If numeric, we should add the embedding function
        }
    }


    // Pre-fetch index. The XMLHttpRequest above will take it from browser cache then
    $("head").append('<link rel="prefetch" href="'+root_path + '/fileIndex'+'">');

/*-------------------------------------------------------------------------------------*/
    // End: print the time that was required to execute the code browser javascript
    elapsed = new Date().getTime() - start;
    console.log("init: " + elapsed);
});

