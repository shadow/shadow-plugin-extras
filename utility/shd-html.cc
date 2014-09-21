/*
 * The Shadow Simulator
 *
 * Copyright (c) 2010-2012 Rob Jansen <jansen@cs.umn.edu>
 *
 * This file is part of Shadow.
 *
 * Shadow is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Shadow is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Shadow.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "shd-html.hpp"
#include <assert.h>
#include <map>
#include <boost/algorithm/string.hpp>

using std::vector;
using std::map;
using std::string;

static string html_parse_img(const map<string, string>& attrs) {
    return attrs.at("src");
}

static string html_parse_link(const map<string, string>& attrs) {
    const string& rel = attrs.at("rel");
    const string& source = attrs.at("href");

    if (rel == "stylesheet") {
        return source;
    }

    if (rel == "shortcut icon") {
        return source;
    }

    return "";
}

static map<string, string>* html_get_attributes(TidyNode node) {
    TidyAttr curr_attr = tidyAttrFirst(node);
    map<string, string>* attrs = new map<string, string>();

    while(curr_attr) {
        gchar* canonical_name = g_utf8_strdown(tidyAttrName(curr_attr), -1);
        const ctmbstr value = tidyAttrValue(curr_attr);
        (*attrs)[string(canonical_name)] = string(value ? value : "");
        g_free(canonical_name);
        curr_attr = tidyAttrNext(curr_attr);
    }

    return attrs;
}

static void html_find_objects(TidyDoc tdoc, TidyNode node,
                              vector<string>& images,
                              vector<ScriptResource>* scripts)
{
    TidyNode child;

    for (child = tidyGetChild(node); child; child = tidyGetNext(child)) {
        map<string, string>* attrs = html_get_attributes(child);
        
        const gchar* name = NULL;
        if ((name = tidyNodeGetName(child))) {
            string url;
            if (g_ascii_strncasecmp(name, "img", 3) == 0) {
                images.push_back(html_parse_img(*attrs));
            } else if (scripts
                       && g_ascii_strncasecmp(name, "script", 6) == 0)
            {
                TidyBuffer buf = {0};
                tidyNodeGetText(tdoc, child, &buf);
                //printf("[%s]___\n", buf.bp);
                string s((const char*)buf.bp, buf.size);
                tidyBufFree(&buf);

                scripts->resize(scripts->size() + 1);
                ScriptResource& sr = scripts->back();
                if (attrs->find("src") != attrs->end()) {
                    sr.src = attrs->at("src");
                }

                boost::trim(s);
                boost::split(sr.lines, s, boost::is_any_of("\n"));
                // remove the first & last lines
                sr.lines.erase(sr.lines.end()); // the "</script>" line
                sr.lines.erase(sr.lines.begin()); // the "<script type= ...>" line
            } else if (g_ascii_strncasecmp(name, "link", 4) == 0) {
                url = html_parse_link(*attrs);
            }
        }
        delete attrs;

        html_find_objects(tdoc, child, images, scripts);
    }
}

void html_parse(const gchar* html, vector<string>& images,
                vector<ScriptResource>* scripts)
{
    TidyDoc tdoc = tidyCreate();
    TidyBuffer tidy_errbuf = {0};
    int err = 0;
  
    tidyOptSetBool(tdoc, TidyForceOutput, yes); /* try harder */ 
    tidyOptSetInt(tdoc, TidyWrapLen, 4096);
    tidySetErrorBuffer( tdoc, &tidy_errbuf );
    
    err = tidyParseString(tdoc, html); /* parse the input */ 
    
    if ( err >= 0 ) {
        err = tidyCleanAndRepair(tdoc); /* fix any problems */ 
        
        if ( err >= 0 ) {
            err = tidyRunDiagnostics(tdoc); /* load tidy error buffer */ 
            
            if ( err >= 0 ) {
                html_find_objects(tdoc, tidyGetHtml(tdoc),
                                  images, scripts); /* walk the tree */ 
            }
        }
    }
    tidyBufFree(&tidy_errbuf);
    tidyRelease(tdoc);
}
