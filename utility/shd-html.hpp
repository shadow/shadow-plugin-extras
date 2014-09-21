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

#ifndef SHD_HTML_HPP_
#define SHD_HTML_HPP_

#include <glib.h>
#include <tidy.h>
#include <tidyenum.h>
#include <buffio.h>
#include <stdint.h>

#include <string>
#include <vector>
#include <map>

class ScriptResource
{
public:
    std::string src;
    std::vector<std::string> lines;
};

void html_parse(const gchar* html, std::vector<std::string>& objs,
                std::vector<ScriptResource>* scripts=NULL);

#endif /* SHD_HTML_HPP_ */
