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


$(function() {

    // remove trailing slash
    root_path = root_path.replace(/\/$/, "");
    if(!root_path) root_path = ".";


    //compute the length of the common prefix between two strings
    // (copied from codebrowser.js)
    var prefixLen = function( s1 , s2) {
        var maxMatchLen = Math.min(s1.length, s2.length);
        var res = -1;
        while (++res < maxMatchLen) {
            if (s1.charAt(res) != s2.charAt(res))
                break;
        }
        return res * 256 + 256 - s1.length;
    }

    // Google text search (different than codebrowser.js)
    var text_search = function(text) {
        var location = "" + (window.location);
        window.location = "http://google.com/search?sitesearch=" + encodeURIComponent(location) + "&q=" + encodeURIComponent(text);
    }

    var fileIndex = [];
    var searchTerms = {}
    var functionDict = {};
    var file = path;

    var searchline = $("input#searchline");

    //BEGIN  copied from codebrowser.js

        // callback for jqueryui's autocomple activate
        var activate = function(event,ui) {
            var val = ui.item.value;
            var type = searchTerms[val] && searchTerms[val].type;
            if (type == "file") {
                window.location = root_path + '/' +  searchTerms[val].file + ".html";
            } else if (type == "ref") {
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

    //END  copied from codebrowser.js


    $.get(root_path + '/fileIndex', function(data) {
        var list = data.split("\n");
        list.sort();

        fileIndex = list;
        for (var i = 0; i < list.length; ++i) {
            searchTerms[list[i]] = { type:"file", file: list[i] };
        }


        function openFolder() {
            var t = $(this);
            var state = {};
            if (history)
                state = history.state || state;
            if (!this._opened) {
                this._opened = true;
                var p = t.attr("data-path") + "/";
                var subPath = path=="" ? p : p.substr(path.length);
                t.text("[-]");
                var content = $("<table/>");
                var dict = {};
                var toOpenNow = [];
                for (var i=0; i < fileIndex.length; ++i) {
                    var f = fileIndex[i];
                    if (f.indexOf(p) == 0) {
                        var sl = f.indexOf('/',  p.length + 1);

                        if (sl !== -1) {
                            var name = f.substr( p.length, sl - p.length);
                            if (dict[name])
                                continue;
                            dict[name] = true;
                            content.append("<tr><td class='folder'><a class='opener' data-path='" + p + name + "'  href='"+subPath + name+"'>[+]</a> " +
                                        "<a href='" + subPath + name + "/'>" + name + "/</a></td></tr>\n");
                            if (state[p+name])
                                toOpenNow.push(p+name);
                        } else {
                            var name = f.substr(p.length);
                            content.append("<tr><td class='file'>    <a href='" + subPath + name + ".html'>" + name + "</a></td></tr>\n");
                        }
                    }
                }
                content.find(".opener").click(openFolder);
                t.parent().append(content);
                state[t.attr("data-path")]=true;
                toOpenNow.forEach(function(toOpen) { 
                    var e = $("a[data-path='"+toOpen+"']").get(0)
                    if (e) 
                        openFolder.call(e);
                });
            } else {
                t.parent().find("> table").empty();
                t.text("[+]");
                this._opened = false;
                state[t.attr("data-path")]=false;
            }
            if (history && history.replaceState)
                history.replaceState(state, undefined);
            return false;
        }

        $(".opener").click(openFolder);
        var state;
        if (history)
            state = history.state;
        if (state) {
            $(".opener").each(function(e) {
                if (state[$(this).attr("data-path")])
                    openFolder.call(this);
            });
        }
    });

    $("#footer").before("<div id='whatisit'><h3>What is this ?</h3><p>This is an online code browser that allows you to browse C/C++ code just like in your IDE, "
                        +  "with <b>semantic highlighting</b> and contextual <b>tooltips</b> that show you the usages and cross references.<br/>"
						+  "Open a C or C++ file and try it by hovering over the symbols!<br />"
						+  "Or take the <a href='https://woboq.com/codebrowser-features.html'>feature tour</a>."
						+  "</p></div>")
});

