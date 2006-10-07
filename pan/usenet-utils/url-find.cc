/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002-2006  Charles Kerr <charles@rebelbase.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <config.h>
#include <cctype>
#include "gnksa.h"
#include "url-find.h"

using namespace pan;

// This is a cheap little hack that should eventually be replaced
// with something more robust.
bool
pan :: url_find (const StringView& text, StringView& setme_url)
{
  if (text.empty())
    return false;

  char bracket (0);
  const char * start (0);
  for (const char *pch (text.begin()), *end(text.end()); pch!=end; ++pch)
  {
    if (*pch=='h' && (end-pch>7) && !memcmp(pch,"http://",7)) {
      start = pch;
      break;
    }
    else if (*pch=='h' && (end-pch>8) && !memcmp(pch,"https://",8)) {
      start = pch;
      break;
    }
    else if (*pch=='f' && (end-pch>6) && !memcmp(pch,"ftp://", 6)) {
      start = pch;
      break;
    }
    else if (*pch=='n' && (end-pch>5) && !memcmp(pch,"news:",5)) {
      start = pch;
      break;
    }
    else if (*pch=='f' && (end-pch>5) && !memcmp(pch,"ftp.",4) && isalpha(pch[4])) {
      start = pch;
      break;
    }
    else if (*pch=='w' && (end-pch>5) && !memcmp(pch,"www.",4) && isalpha(pch[4])) {
      start = pch;
      break;
    }
    else if (*pch=='@') {
      const char *b, *e;
      for (b=pch; b!=text.str && !isspace(b[-1]); ) --b;
      for (e=pch; e!=text.end() && !isspace(e[1]); ) ++e;
      StringView v (b, e+1-b);
      while (!v.empty() && strchr(">?!.,", v.back())) --v.len;
      if (GNKSA::check_from(v,false) == GNKSA::OK) {
        setme_url = v;
        return true;
      }
    }
  }

  if (!start)
    return false;

  if (start != text.begin()) {
    char ch (start[-1]);
    if (ch == '[') bracket = ']';
    else if (ch == '<') bracket = '>';
  } 

  const char * pch;
  for (pch=start; pch!=text.end(); ++pch) {
    if (bracket) {
      if (*pch == bracket)
        break;
    }
    else if (isspace(*pch) || strchr("{}()|[]<>",*pch))
      break;
  }

  setme_url.assign (start, pch-start);

  // for urls at the end of a sentence.
  if (!setme_url.empty() && strchr("?!.,", setme_url.back()))
    --setme_url.len;

  return true;
}