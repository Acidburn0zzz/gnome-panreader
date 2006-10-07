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
#include <ctime>
#include <glib/gi18n.h>
#include <pan/general/debug.h>
#include <pan/general/foreach.h>
#include "nntp-pool.h"

using namespace pan;

NNTP_Pool :: NNTP_Pool (const Quark        & server,
                        ServerInfo         & server_info,
                        Socket::Creator    * creator):
  _server_info (server_info),
  _server (server),
  _socket_creator (creator),
  _connection_pending (false),
  _max_connections (_server_info.get_server_limits (_server)),
  _active_count (0)
{
  _server_info.add_listener (this);
}

NNTP_Pool :: ~NNTP_Pool ()
{
  _server_info.remove_listener (this);

  foreach (pool_items_t, _pool_items, it) {
    delete it->nntp->_socket;
    delete it->nntp;
  }
}

/***
****
***/

void
NNTP_Pool :: abort_tasks ()
{
  foreach (pool_items_t, _pool_items, it)
    if (!it->is_checked_in)
      it->nntp->_socket->set_abort_flag (true);
}

NNTP*
NNTP_Pool :: check_out ()
{
  NNTP * nntp (0);

  foreach (pool_items_t, _pool_items, it) {
    if (it->is_checked_in) {
      nntp = it->nntp;
      it->is_checked_in = false;
      ++_active_count;
      debug ("nntp " << nntp << " is now checked out");
      break;
    }
  }

  return nntp;
}

void
NNTP_Pool :: check_in (NNTP * nntp, bool is_ok)
{
  debug ("nntp " << nntp << " is being checked in, is_ok is " << is_ok);

  // find this nntp in _pool_items
  pool_items_t::iterator it;
  for (it=_pool_items.begin(); it!=_pool_items.end(); ++it)
    if (it->nntp == nntp)
      break;

  // process the nntp if we have a match
  if (it != _pool_items.end()) {
    --_active_count;
    if (is_ok) {
      it->is_checked_in = true;
      it->last_active_time = time (NULL);
      fire_pool_has_nntp_available ();
    } else {
      delete it->nntp->_socket;
      delete it->nntp;
      _pool_items.erase (it);
    }
  }
}

/***
****
***/

void
NNTP_Pool :: on_server_limits_changed (const Quark& server, int max_connections)
{
  assert (server == _server);
  _max_connections = max_connections;
}

/***
****
***/

void
NNTP_Pool :: on_socket_created (const StringView& host, int port, bool ok, Socket* socket)
{
  if (!ok)
  {
    delete socket;
    _connection_pending = false;
  }
  else
  {
    // okay, we at least we established a connection.
    // now try to handshake and pass the buck to on_nntp_done().
    std::string user, pass;
    _server_info.get_server_auth (_server, user, pass);
    NNTP * nntp = new NNTP (_server, user, pass, socket);
    nntp->handshake (this); // 
  }
}

void
NNTP_Pool :: on_nntp_done (NNTP* nntp, Health health)
{
   debug ("NNTP_Pool: on_nntp_done()");

   if (health != OK)
   {
      delete nntp->_socket;
      delete nntp;
      nntp = 0;
   }

   if (health == FAIL) // news server isn't accepting our connection!
   {
     const std::string addr (_server_info.get_server_address (_server));
     char buf[512];
     snprintf (buf, sizeof(buf), _("Unable to connect to \"%s\""), addr.c_str());
     fire_pool_error (buf);
   }

   _connection_pending = false;

   // if success...
   if (nntp != 0)
   {
      debug ("success with handshake to " << _server << ", nntp " << nntp);

      PoolItem i;
      i.nntp = nntp;
      i.is_checked_in = true;
      i.last_active_time = time (0);
      _pool_items.push_back (i);

      fire_pool_has_nntp_available ();
   }
}

void
NNTP_Pool :: get_counts (int& setme_active,
                         int& setme_idle,
                         int& setme_pending,
                         int& setme_max) const
{
  setme_active = _active_count;
  setme_idle = _pool_items.size() - _active_count;
  setme_max = _max_connections;
  setme_pending  = _connection_pending ? 1 : 0;
}


void
NNTP_Pool :: request_nntp ()
{
  int active, idle, pending, max;
  get_counts (active, idle, pending, max);

#if 0
  std::cerr << LINE_ID << "server " << _server << ", "
            << "active: " << active << ' '
            << "idle: " << idle << ' '
            << "pending: " << pending << ' '
            << "max: " << max << ' ' << std::endl;
#endif

  if (!idle && !pending && active<max)
  {
    debug ("trying to create a socket");

    _connection_pending = true;
    std::string address;
    int port;
    _server_info.get_server_addr (_server, address, port);
    _socket_creator->create_socket (address, port, this);
  }
}

/**
***
**/

static const int MAX_IDLE_SECS (30);

static const int HUP_IDLE_SECS (90);

namespace
{
  class NoopListener: public NNTP::Listener
  {
    private:
      NNTP::Source * source;
      const bool force_not_ok;

    public:
      NoopListener (NNTP::Source * s, bool b): source(s), force_not_ok(b) {}
      virtual ~NoopListener() {}
      virtual void on_nntp_done  (NNTP * nntp, Health health) {
        source->check_in (nntp, (health==OK) && !force_not_ok);
        delete this;
      }
  };
}

void
NNTP_Pool :: idle_upkeep ()
{
  for (;;)
  {
    PoolItem * item (0);

    const time_t now (time (0));
    foreach (pool_items_t, _pool_items, it) {
      if (it->is_checked_in && ((now - it->last_active_time) > MAX_IDLE_SECS)) {
        item = &*it;
        break;
      }
    }

    // if no old, checked-in items, then we're done
    if (!item)
      break;

    // send a keepalive message to the old, checked-in item we found
    // the noop can trigger changes in _pool_items, so that must be
    // the last thing we do with the 'item' pointer.
    const time_t idle_time_secs = now - item->last_active_time;
    item->is_checked_in = false;
    ++_active_count;
    if (idle_time_secs >= HUP_IDLE_SECS)
      item->nntp->goodbye (new NoopListener (this, true));
    else
      item->nntp->noop (new NoopListener (this, false));
    item = 0;
  }
}