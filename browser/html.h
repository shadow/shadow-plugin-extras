/*
 * The Shadow Simulator
 * See LICENSE for licensing information
 */

#ifndef SHD_HTML_H_
#define SHD_HTML_H_

#include <glib.h>
#include <tidy.h>
#include <tidyenum.h>
#include <buffio.h>

void html_parse(const gchar* html, GSList** objs);

#endif /* SHD_HTML_H_ */
