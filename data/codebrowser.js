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

//Styles:
var setStyle = "";
document.write("<link rel='alternate stylesheet' title='Solarized' href='" +root_path + "/../data/solarized.css' />"); //FIXME: data url
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
    var c = readCookie('style');
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

    var escape_selector = function (str) {
        return str.replace(/([ #;&,.+*~\':"!^$[\]()=<>|\/@{}])/g,'\\$1')
    }

    function escape_html(str) {
        return $("<p/>").text(str).html();
    }

    // demangle the function name, don't care about the template or the argument
    function demangleFunctionName(mangle) {
        if (! mangle) return mangle;
        if (mangle[0] !== '_') return mangle;
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
        } while(mangle && mangle[0]!='E' && scoped);
        return result;
    }

    // ident and highlight code (for macros)
    function identAndHighlightMacro(origin) {
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
                    } else if (string == 1 && (origin.charAt(i-1) != '\\' || origin.charAt(i-2) == '\\')) {
                        string = 0;
                        result += "\"</q>"
                    }
                    break;
                case '\'':
                    if (string == 0) {
                        result += "<kdb>\'"
                        string = 2;
                    } else if (string == 2 && (origin.charAt(i-1) != '\\' || origin.charAt(i-2) == '\\')) {
                        string = 0;
                        result += "\'</kdb>"
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
    var prefixLen = function( s1 , s2) {
        var maxMatchLen = Math.min(s1.length, s2.length);
        res = -1;
        while (++res < maxMatchLen) {
            if (s1.charAt(res) != s2.charAt(res))
                break;
        }
        return res * 256 + 256 - s1.length;
    }

    var tooltip = {
        ref: "", //the 'ref' of the current symbol displayed
        showTimerId : null,
        hideTimerId : null,
        tooltip : {}, // set when the document is initialized
        showDelay : 350,
        hideDelay : 200,
        gap : 12,

        init: function() {
            $("div#content").append("<div id='tooltip' style='position:absolute' />");
            this.tooltip = $("#tooltip");
            var this_ = this;
            this.tooltip.hover(
                function () { clearTimeout(this_.hideTimerId);  },
                function () { this_.hideAfterDelay(); }
            );
        },

        setUnderElem: function(elem) {
            var content=$("div#content")
            var docwidth = content.innerWidth()-15;
            var docheight= content.innerHeight()-18;
            var twidth=this.tooltip.get(0).offsetWidth;
            var theight=this.tooltip.get(0).offsetHeight;
            var tipx=elem.position().left + elem.width()/2 - twidth/2 ;
            tipx += content.scrollLeft();
            if (tipx+twidth>docwidth) tipx = docwidth - twidth - this.gap;
            else if (tipx < 0) tipx = this.gap;
            var tipy=elem.position().top + elem.height()/2 + this.gap;
            tipy += content.scrollTop();
            tipy=(tipy-content.scrollTop()+theight>docheight && tipy-theight>content.scrollTop()) ? tipy-theight-(2*this.gap) : tipy //account for bottom edge
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


    // Compatibility with code browser <= 1.3:   generate the div#content
    if ($("body > #content").length != 1) {
        $("body > :not(#mask, #header, #tooltip, #header+hr)").wrapAll("<div id='content' />");
    }

    tooltip.init();

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
        highlighted_items = $("[data-ref='"+escape_selector(ref)+"']");
        highlighted_items.addClass("highlight")
    }

    var anchor_id  = location.hash.substr(1); //Get the word after the hash from the url
    if (/^\d+$/.test(anchor_id)) {
        highlighted_items = $("#" + anchor_id);
        highlighted_items.addClass("highlight")
    } else if (anchor_id != "") {
        highlight_items(anchor_id);
    }
    var skipHighlightTimerId = null;
    skipHighlightTimerId = setTimeout(function() { skipHighlightTimerId = null }, 600)

/*-------------------------------------------------------------------------------------*/
    var onMouseLeave = function(e) { tooltip.hideAfterDelay(e); }
    var onMouseClick = function(e) {
        tooltip.tooltip.hide();
        skipHighlightTimerId = setTimeout(function() { skipHighlightTimerId = null }, 600);

        if (history && history.pushState && shouldFade && this.href) {
            var href = this.href;
            var hashPos = href.indexOf("#");
            if (hashPos >= 0) {
                var anchor = href.substr(hashPos+1);
                var url = href.substr(0, hashPos);
                if (url == "" || url === location.origin + location.pathname) {
                    var target = $("#" + escape_selector(anchor));
                    if (target.length == 1) {
                        history.pushState({}, undefined, href); //so back goes back to the last location
                        $("#content").animate({scrollTop:target.position().top + $("#content").scrollTop() }, 300);
                        e.preventDefault();
                        return false;
                    }
                }
            }
        }
        return true;
    }

    // Mouse interaction (tooltip, ...)
    var onMouseEnterRef = function(e) {
            if (skipHighlightTimerId) return false;
            var elem = $(this);
            var ref = elem.attr("data-ref");
            var proj = elem.attr("data-proj");

            var proj_root_path = root_path;
            if (proj) { proj_root_path = projects[proj]; }

            var url = proj_root_path + "/refs/" + ref;

            if (!$(this).hasClass("highlight")) {
                highlight_items(ref);
            }

            var computeTooltipContent = function(data, title, id) {
                var type ="", content ="";
                if (elem.hasClass("local")) {
                    type = $("#" + escape_selector(ref)).attr("data-type");
                    content = "<br/>(local)";
                } else if (elem.hasClass("tu")) {
                    type = $("#" + escape_selector(ref)).attr("data-type");

                    var docs = $("i[data-doc='"+escape_selector(ref)+"']");
                    docs.each(function() {
                        var comment = $(this).html();
                        content += "<br/><i>" + comment + "</i>";
                    });

                    //var uses = highlighted_items;
                    var uses = $("[data-ref='"+escape_selector(ref)+"']");
                    var usesLis ="";
                    var usesCount = 0;
                    uses.each(function() {
                        var t = $(this);
                        var l = t.parent().prev("th").text();

                        if (t.hasClass("def")) {
                            content += "<br/><a href='#"+ l +"'>Definition</a>";
                        } else if (t.hasClass("decl")) {
                            content += "<br/><a href='#"+ l +"'>Declaration</a>";
                        } else {
                            var c;
                            var context = t.closest("tr").prevAll().find(".def").first();
                            if (context.length == 1 && context.hasClass("decl")) {
                                c = context[0].title_;
                                if (c === undefined)
                                    c = context.attr("title")
                            }
                            if (!c) c = "line " + l;
                            usesLis += "<li><a href='#"+ l +"'>"+ escape_html(c) +"</a></li>"
                            usesCount += 1;
                        }
                    });

                    if (usesCount > 0)
                        content += "<br/><a href='#' class='showuse'>Show Uses:</a> (" + usesCount + ")<br/><ul class='uses'>" + usesLis + "</ul>"

                } else if (elem.hasClass("typedef")) {
                    type = elem.attr("data-type");
                } else {
                    var res = $("<data>"+data+"</data>");

                    var typePrefixLen = -1;

                    //comments:
                    var seen_comments = [];
                    res.find("doc").each(function() {
                        var comment = $(this).html();
                        if ($.inArray(comment, seen_comments) !== -1)
                            return;
                        content += "<br/><i>" + comment + "</i>";
                        seen_comments.push(comment);
                    });

                    var p = function (label, tag) {
                        var d = res.find(tag);
                        if (!d.length)
                            return;
                        content += "<br/>" + label + ": (" + d.length + ")";
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
                                if (!dict[f]) {
                                    dict[f] = [];
                                    dict.number++;
                                }
                                dict[f].push(l);
                            } else {
                                var url = proj_root_path + "/" + f + ".html#" + l;
                                content += "<br/><a href='" + url +"' >" + f + ":" + l + "</a>";
                                if (tag === "ovr") {
                                    var c = th.attr("c");
                                    if (c)
                                        content += " (" + demangleFunctionName(c) + ")";
                                }
                            }
                        });
                        if (shouldCompress) {
                            if (dict.number > 20) {
                                content += "<br/>(Too many)";
                                return;
                            }
                            for(var f in dict) {
                                if (!dict.hasOwnProperty(f) || f==="number") continue;
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
                    }
                    p("Definitions", "def");
                    p("Declarations", "dec");
                    p("Overriden", "ovr");

                    // Uses:
                    var uses = res.find("use");
                    if (uses.length) {
                        var dict = { };
                        uses.each(function() {
                            var t = $(this);
                            var f = t.attr("f");
                            var l = t.attr("l");
                            var c = t.attr("c");
                            var url = proj_root_path + "/" + f + ".html#" + l;
                            if (!dict[f]) {
                                dict[f] = { elem: $("<li/>").append($("<a/>").attr("href", url).text(f)),
                                            contexts: {},  prefixL: prefixLen(file, f), count: 0};
                            }
                            c = demangleFunctionName(c)
                            if (!c) c = f + ":" + l;
                            dict[f].count++;
                            if (!dict[f].contexts[c]) {
                                dict[f].contexts[c] = $("<li/>").append($("<a/>").attr("href", url).text(c));
                                dict[f].contexts[c].count = 1;
                            } else {
                                dict[f].contexts[c].count++;
                            }
                        });
                        var list = [];
                        for (var xx in dict) {
                            if (dict.hasOwnProperty(xx))
                                list.push(dict[xx]);
                        }
                        list.sort(function(a,b){ var dif = b.prefixL - a.prefixL; return dif ? dif : (b.f - a.f) });
                        var ul = $("<ul class='uses'/>");
                        for (var i = 0; i < list.length; ++i) {
                            var subul = $("<ul/>");
                            for (var xx in list[i].contexts) {
                                if (list[i].contexts.hasOwnProperty(xx))
                                    subul.append(list[i].contexts[xx].append(" (" + list[i].contexts[xx].count+")"));
                            }
                            ul.append(list[i].elem.append(" (" + list[i].count+")").append(subul));
                        }
                        content += "<br/><a href='#' class='showuse'>Show Uses:</a> (" + uses.length + ")<br/>" + $("<span>").append(ul).html();
                    }
                }

                var tt = tooltip.tooltip;
                tt.empty();
                if (id && id != "") {
                    tt.append($("<b />").append($("<a class='link' href='#"+ id +"' />").text(title)));
                } else {
                    tt.append($("<b />").text(title));
                }
                if (type != "") {
                    tt.append("<br/>");
                    tt.append($("<span class='type' />").text(type));
                }
                tt.append($("<span />").html(content));
                tooltip.ref = ref;
                tt.find(".uses").hide();
                tt.find(".showuse").click(function(e) {
                    tt.find(".uses").toggle(); return false;});
            }

            if (!this.title_) {
                this.title_ = elem.attr("title");
                elem.removeAttr("title");
            }

            var tt = this;
            if (!this.tooltip_loaded && !elem.hasClass("local") && !elem.hasClass("tu") && !elem.hasClass("typedef")) {
                this.tooltip_loaded = true;
                $.get(url, function(data) {
                    tt.tooltip_data = data;
                    if (tooltip.ref === ref)
                        computeTooltipContent(data, tt.title_, tt.id);

                    // attempt to change the href to the definition
                    var res = $("<data>"+data+"</data>");
                    var def =  res.find("def");
                    if (def.length > 0) {

                        var currentLine = elem.parents("tr").find("th").text();
                        //if there are several definition we take the one closer in the hierarchy.
                        var result = {  len: -1 };
                        def.each( function() {
                            var cur = { len : -1,
                                        f : $(this).attr("f"),
                                        l : $(this).attr("l") }


                            if (cur.f === file && cur.l === currentLine)
                                return;

                            cur.len = prefixLen(cur.f, file)
                            if (cur.len >= result.len) {
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

        // Macro tooltip
        var onMouseEnterMacro = function(e) {
            if (skipHighlightTimerId) return false;
            if (highlighted_items) {
                highlighted_items.removeClass("highlight");
                highlighted_items = null;
            }
            var elem = $(this);
            if (this.title_ === undefined) {
                this.title_ = elem.attr("title");
                elem.removeAttr("title");
                if (this.title_ === undefined)
                    return;
                this.title_ = identAndHighlightMacro(this.title_);
            }
            var expansion = this.title_;

            function computeMacroTooltipContent() {
                var tt = tooltip.tooltip;
                tt.empty();
                tt.append($("<code class='code' style='white-space: pre-wrap' />").html(expansion));
            }
            tooltip.showAfterDelay(elem, computeMacroTooltipContent);
        }

        //function is_touch_device() { return !!('ontouchstart' in window); }
        function is_touch_device() {
            return !!('ontouchstart' in document.documentElement) &&
                // Arf, Konqueror support touch, even on desktop
                (!navigator || (navigator.userAgent.indexOf("konqueror")===-1 && navigator.userAgent.indexOf("rekonq")===-1));
        }
        if (is_touch_device()) {
            var elemWithTooltip;
            function applyTo(func) { return function() {
                if (this === elemWithTooltip) {
                    return onMouseClick.apply(this, arguments);
                } else {
                    elemWithTooltip = this;
                    var oldDelay = tooltip.showDelay;
                    tooltip.showDelay = 1;
                    func.apply(this, arguments);
                    tooltip.showDelay = oldDelay;
                    return false;
                }
            }; };
            $(".code").on({"click": applyTo(onMouseEnterRef) }, "[data-ref]");
            $(".code").on({"click": applyTo(onMouseEnterMacro) }, ".macro");
        } else {
            $(".code").on({"mouseenter": onMouseEnterRef, "mouseleave": onMouseLeave, "click": onMouseClick},
                        "[data-ref]");
            $(".code").on({"mouseenter": onMouseEnterMacro, "mouseleave": onMouseLeave, "click": onMouseClick},
                          ".macro");
        }
        tooltip.tooltip.on({"click": onMouseClick}, "a")

/*-------------------------------------------------------------------------------------*/

    //bread crumbs.
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
    bread += "<br/><span id='breadcrumb_symbol' </p>";
    $("#header").append(bread);


/*-------------------------------------------------------------------------------------*/
    
    // Search Line
    var searchTerms;
    $("#header").prepend("<input id='searchline' type='text' placeholder='Search a file'/>");
    $("#searchline").focus(function() {
        if (searchTerms)
            return;
        searchTerms = {}

        function reset()  {
            var activate = function(event,ui) {
                var val = ui.item.value;
                if (searchTerms[val].type == "file") {
                    window.location = root_path + '/' +  searchTerms[val].file + ".html";
                } else if (searchTerms[val].type == "ref") {
                    var ref = searchTerms[val].ref;
                    var url = root_path + "/refs/" + ref;
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
                }
            };

            var text_search = function(text) {
                var location = "" + (window.location);
                var idx = location.indexOf(file);
                if (idx < 0)
                    return;
                location = location.substring(0, idx);
                window.location = "http://google.com/search?sitesearch=" + encodeURIComponent(location) + "&q=" + encodeURIComponent(text);
            }

            var list = [];
            for (xx in searchTerms) {
                if (searchTerms.hasOwnProperty(xx))
                    list.push(xx);
            }
            $("#searchline").autocomplete( {source: list, select: activate, minLength: 4  } );
            $("#searchline").keypress( function(e) { if(e.which == 13) {
                        var val = $("#searchline").val();
                        if ( $.inArray(val, list) < 0) {
                            text_search(val);
                        } else {
                            window.location = root_path + "/" + val + ".html";
                        }
                } } );
        }

        $.get(root_path + '/fileIndex', function(data) {
            var list = data.split("\n");
            for (var i = 0; i < list.length; ++i) {
                searchTerms[list[i]] = { type:"file", file: list[i] };
            }
            reset();
        });

     /*   $.get(root_path + '/' + project + '/functionIndex', function(data) {
            var list = data.split("\n");
            for (var i = 0; i < list.length; ++i) {
                var coma = list[i].indexOf(',');
                var ref = list[i].slice(0, coma);
                var name = list[i].slice(coma+1);
                searchTerms[name] = { type:"ref", ref: ref };
            }
            reset();
        });*/
        reset();
        return false;
    });

/*-------------------------------------------------------------------------------------*/

    // Find the current context while scrolling
    $("#content").scroll(function() {
        var toppos = $("#content").offset().top;
        $('tr').each(function() {
            //console.log( scrolltop, $(this).offset().top,  $(this).text()  )
            if ($(this).offset().top > toppos) {
                var context = $(this).prevAll().find(".def").first();
                var c = "";
                if (context.length == 1 && context.hasClass("decl")) {
                    c = context[0].title_;
                    if (c === undefined)
                        c = context.attr("title");
                }

                $("span#breadcrumb_symbol").text(c);


                return false;
            }
        });
    });

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
    $(".opt_close").click(function() {
        $("#options_dlg").dialog('close');
            return false;
    });

    $("#header").prepend("<a class='logo' href='http://code.woboq.org'><img src='http://code.woboq.org/data/woboq-48.png'/></a>");

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

    elapsed = new Date().getTime() - start;
    console.log("init: " + elapsed);
});

//*******************************************
// Google analytics

var _gaq = _gaq || [];
_gaq.push(['_setAccount', 'UA-221649-12']);
_gaq.push(['_setDomainName', 'woboq.org']);
_gaq.push(['_setAllowLinker', true]);
_gaq.push(['_trackPageview']);

(function() {
    var ga = document.createElement('script'); ga.type = 'text/javascript'; ga.async = true;
    ga.src = ('https:' == document.location.protocol ? 'https://ssl' : 'http://www') + '.google-analytics.com/ga.js';
    var s = document.getElementsByTagName('script')[0]; s.parentNode.insertBefore(ga, s);
})();

// adds
if ($(window).width() > 600) {
    if (document.referrer.indexOf("google") !== -1) {
        document.write("<div style='display:none; visibility: hidden'>"+
            "<div id='googlead' style='position: absolute; right: 0; margin:1em; z-index:0'>"+
            "<script type='text/javascript'>google_ad_client = 'ca-pub-5892035981328708'; google_ad_slot = '6278880490'; google_ad_width = 200; google_ad_height = 200; </script>" +
            "<script type=\"text/javascript\" src=\"http://pagead2.googlesyndication.com/pagead/show_ads.js\">  </script>" +
            "</div></div>");
        $(function() {
            $("table.code").before($("#googlead"));
        });
    } else if (document.referrer.indexOf("woboq") == -1) {
        $(function() {
            $("table.code").before("<div><div id='banner' style='position: absolute; right: 0; margin:1em; z-index:0'><a href='http://woboq.com/codebrowser.html?b'>"+
                "<img src='http://code.woboq.org/codebrowserbanner.png' style='border:0; box-shadow: 1px 1px 3px #969696;'/></a></div></div>");
            banner =  $("#banner img");
            banner.hover(
                function () {
                    banner.css({'box-shadow' : '1px 1px 3px #037',
                            'position' : 'relative',
                            'top' : 1 , 'left': 1 });
                }, function () {
                    banner.css({'box-shadow' : '1px 1px 3px #969696',
                            'top' : 0 , 'left': 0 });
                }
            );
        });
    }
}