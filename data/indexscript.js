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


$(function() {

    var fileIndex;


    $.get(root_path + 'fileIndex', function(data) {
        var list = data.split("\n");
        list.sort();

        var activate = function(event,ui) {
            var val = ui.item.value;
            window.location = root_path +  val + ".html";
        }

        $("#searchline").autocomplete( {source: list, select: activate, minLength:4  } );

        fileIndex = list;

        function openFolder() {
            var t = $(this);
            if (!this._opened) {
                this._opened = true;
                var p = t.attr("data-path") + "/";
                var subPath = path=="" ? p : p.substr(path.length);
                t.text("[-]");
                var content = $("<table/>");
                var dict = {};
                for (var i=0; i < fileIndex.length; ++i) {
                    var f = fileIndex[i];
                    if (f.indexOf(p) == 0) {
                        var sl = f.indexOf('/',  p.length + 1);

                        if (sl !== -1) {
                            var name = f.substr( p.length, sl - p.length);
                            if (dict[name])
                                continue;
                            dict[name] = true;
                            content.append("<tr><td class='folder'><a class='opener' data-path='" + p + name + "' + href='"+subPath + name+"'>[+]</a> " +
                                        "<a href='" + subPath + name + "/'>" + name + "/</a></td></tr>\n");
                        } else {
                            var name = f.substr(p.length);
                            console.log(name);
                            content.append("<tr><td>    <a href='" + subPath + name + ".html'>" + name + "</a></td></tr>\n");
                        }
                    }
                }
                content.find(".opener").click(openFolder);
                t.parent().append(content);
            } else {
                t.parent().find("> table").empty();
                t.text("[+]");
                this._opened = false;
            }
            return false;
        }

        $(".opener").click(openFolder);
    });
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
