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
#include <cassert>
#include <pan/general/debug.h>
#include <pan/general/foreach.h>
#include <pan/data/data.h>
#include "article-filter.h"

using namespace pan;

StringView
ArticleFilter :: get_header (const Article& a, const Quark& header_name) const
{
  StringView value;

  if      (header_name == subject)    value = a.subject.c_str();
  else if (header_name == from)       value = a.author.c_str();
  else if (header_name == message_Id) value = a.message_id.c_str();
  else if (header_name == message_ID) value = a.message_id.c_str();
  else {
    std::cerr << LINE_ID << ' ' << PACKAGE_STRING << " is misparsing \"" << header_name << "\".\n"
              << "Please file a bug report to http://bugzilla.gnome.org/enter_bug.cgi?product=Pan\n";
    value = "";
  }

  return value;
}


bool
ArticleFilter :: test_article (const Data        & data,
                               const FilterInfo  & criteria,
                               const Quark       & group,
                               const Article     & article) const
{
  bool pass (false);
  switch (criteria._type)
  {
    case FilterInfo::AGGREGATE_AND:
      pass = true;
      foreach_const (FilterInfo::aggregates_t, criteria._aggregates, it) {
        if (!test_article (data, *it, group, article)) {
          pass = false;
          break;
        }
      }
      break;

    case FilterInfo::AGGREGATE_OR:
      if (criteria._aggregates.empty())
        pass = true;
      else {
        pass = false;
        foreach_const (FilterInfo::aggregates_t, criteria._aggregates, it) {
          if (test_article (data, *it, group, article)) {
            pass = true;
            break;
          }
        }
      }
      break;

    case FilterInfo::IS_BINARY:
      pass = article.get_part_state() == Article::COMPLETE;
      break;

    case FilterInfo::IS_POSTED_BY_ME:
      pass = data.has_from_header (article.author.to_view());
      break;

    case FilterInfo::IS_UNREAD:
      pass = !data.is_read (&article);
      break;

    case FilterInfo::BYTE_COUNT_GE:
      pass = article.is_byte_count_ge ((unsigned long)criteria._ge);
      break;

    case FilterInfo::CROSSPOST_COUNT_GE: {
      quarks_t groups;
      foreach_const (Xref, article.xref, xit)
        groups.insert (xit->group);
      pass = (int)groups.size() >= criteria._ge;
      break;
    }

    case FilterInfo::DAYS_OLD_GE:
      pass = (time(NULL) - article.time_posted) > (criteria._ge * 86400);
      break;

    case FilterInfo::LINE_COUNT_GE:
      pass = article.is_line_count_ge ((unsigned int)criteria._ge);
      break;

    case FilterInfo::TEXT:
      if (criteria._header == xref)
      {
        if (criteria._text._impl_type == TextMatch::CONTAINS) // user is filtering by groupname?
        {
          foreach_const (Xref, article.xref, xit)
            if ((pass = criteria._text.test (xit->group.to_view())))
              break;
        }
        else if (criteria._text._impl_text.find(".*:.*") != std::string::npos) // user is filtering by # of crossposts?
        {
          const StringView v (criteria._text._impl_text);
          FilterInfo tmp;
          tmp.set_type_crosspost_count_ge (std::count (v.begin(), v.end(), ':'));
          pass = test_article (data, tmp, group, article);
        }
        else // oh fine, then, user is doing some other damn thing with the xref header.  build one for them.
        {
          std::string s;
          foreach_const (Xref, article.xref, xit) {
            if (s.empty()) {
              int unused;
              data.get_server_addr (xit->server, s, unused);
              s += ' ';
            }
            s += xit->group;
            s += ':';
            char buf[32];
            snprintf (buf, sizeof(buf), "%lu", xit->number);
            s += buf;
            s += ' ';
          }
          if (!s.empty())
            s.resize (s.size()-1);
          pass = criteria._text.test (s);
        }
      }
      else if (criteria._header == newsgroups)
      {
        quarks_t unique_groups;
        foreach_const (Xref, article.xref, xit)
          unique_groups.insert (xit->group);

        std::string s;
        foreach_const (quarks_t, unique_groups, git) {
          s += git->c_str();
          s += ',';
        }
        if (!s.empty())
          s.resize (s.size()-1);
        pass = criteria._text.test (s);
      }
      else if (criteria._header == references)
      {
        std::string s;
        data.get_article_references (group, &article, s);
        pass = criteria._text.test (s);
      }
      else
        pass = criteria._text.test (get_header(article, criteria._header));
      break;

    case FilterInfo::SCORE_GE:
      pass = article.score >= criteria._ge;
      break;

    case FilterInfo::IS_CACHED:
      pass = data.get_cache().contains (article.message_id);
      break;

    case FilterInfo::TYPE_ERR:
      assert (0 && "invalid type!");
      pass = false;
      break;
  }

  if (criteria._negate)
    pass = !pass;

  return pass;
}

void
ArticleFilter :: test_articles (const Data        & data,
                                const FilterInfo  & criteria,
                                const Quark       & group,
                                const articles_t  & in,
                                articles_t        & setme_pass,
                                articles_t        & setme_fail) const
{
  foreach_const (articles_t, in, it) {
    const Article * a (*it);
    if (test_article (data, criteria, group, *a))
      setme_pass.push_back (a);
    else
      setme_fail.push_back (a);
  }
}

int
ArticleFilter :: score_article (const Data         & data,
                                const sections_t   & sections,
                                const Quark        & group,
                                const Article      & article) const
{
  int score (0);
  foreach_const (sections_t, sections, sit) {
    const Scorefile::Section * section (*sit);
    foreach_const (Scorefile::items_t, section->items, it) {
      if (it->expired)
        continue;
      if (!test_article (data, it->test, group, article))
        continue;
      if (it->value_assign_flag)
        return it->value;
      score += it->value;
    }
  }
  return score;
}

void
ArticleFilter :: get_article_scores (const Data          & data,
                                     const sections_t    & sections,
                                     const Quark         & group,
                                     const Article       & article,
                                     Scorefile::items_t  & setme) const
{
  foreach_const (sections_t, sections, sit) {
    const Scorefile::Section * section (*sit);
    foreach_const (Scorefile::items_t, section->items, it) {
      if (it->expired)
        continue;
      if (!test_article (data, it->test, group, article))
        continue;
      setme.push_back (*it);
      if (it->value_assign_flag)
        return;
    }
  }
}