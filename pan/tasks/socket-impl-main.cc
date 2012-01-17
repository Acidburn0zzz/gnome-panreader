
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002-2006  Charles Kerr <charles@rebelbase.com>
 *
 * This file
 * Copyright (C) 2011 Heinrich M�ller <sphemuel@stud.informatik.uni-erlangen.de>
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


/***
****  GIOChannel::SocketCreator -- create a socket in a worker thread
***/

#include <string>
//#include <glib/giochannel.h>
//#include <glib/gstring.h>

extern "C"
{
  #include <glib.h>
}

#include <pan/tasks/socket.h>

#include <config.h>
#include <iostream>
#include <string>
#include <cerrno>
#include <cstring>

#ifdef HAVE_GNUTLS
  #include <pan/usenet-utils/ssl-utils.h>
#endif

#include <pan/general/debug.h>
#include <pan/general/log.h>
#include <pan/general/locking.h>
#include <pan/general/macros.h>
#include <pan/general/worker-pool.h>
#include <pan/general/string-view.h>
#include "socket-impl-main.h"

#ifdef G_OS_WIN32
  #undef _WIN32_WINNT
  #define _WIN32_WINNT 0x0501
  #include <ws2tcpip.h>
  #undef gai_strerror
#endif

using namespace pan;

namespace
{
  const unsigned int TIMEOUT_SECS (10);

  struct ThreadWorker : public WorkerPool::Worker,
                        public WorkerPool::Worker::Listener
  {
    std::string host;
    int port;
    Socket::Creator::Listener * listener;

    bool ok;
    ServerInfo& data;
    Socket * socket;
    std::string err;
    bool use_ssl;
    const Quark server;
#ifdef HAVE_GNUTLS
    CertStore& store;
    ThreadWorker (ServerInfo& d, const Quark& s, const StringView& h, int p, Socket::Creator::Listener *l,
                  bool ssl, CertStore& cs):
      data(d), server(s), host(h), port(p), listener(l), ok(false), socket(0), use_ssl(ssl), store(cs) {}
#else
    ThreadWorker (ServerInfo& d, const Quark& s, const StringView& h, int p, Socket::Creator::Listener *l):
      data(d), server(s), host(h), port(p), listener(l), ok(false), socket(0), use_ssl(false) {}
#endif

    void do_work ()
    {
#ifdef HAVE_GNUTLS
        if (use_ssl)
        {
          socket = new GIOChannelSocketGnuTLS (data, server, store);
        }
        else
#endif
          socket = new GIOChannelSocket ();
      ok = socket->open (host, port, err);
    }

    /** called in main thread after do_work() is done */
    void on_worker_done (bool cancelled UNUSED)
    {
      // pass results to main thread...
      if (!err.empty())   Log :: add_err (err.c_str());
      listener->on_socket_created (host, port, ok, socket);
    }
  };
}


SocketCreator :: SocketCreator(Data& d, CertStore& cs) : data(d), store(cs)
{

#ifdef HAVE_GNUTLS
  gnutls_global_init();
  cs.add_listener(this);
  cs.init();
#endif
}


SocketCreator :: ~SocketCreator()
{
#ifdef HAVE_GNUTLS
  gnutls_global_deinit();
  store.remove_listener(this);
#endif
}

void
SocketCreator :: create_socket (ServerInfo& info,
                                const StringView & host,
                                int                port,
                                WorkerPool       & threadpool,
                                Socket::Creator::Listener * listener,
                                bool               use_ssl)
{
    Quark server;
    data.find_server_by_hn(host, server);
    ensure_module_init ();
    if (store.in_blacklist(server)) return;
#ifdef HAVE_GNUTLS
    ThreadWorker * w = new ThreadWorker (info, server, host, port, listener, use_ssl, store);
#else
    ThreadWorker * w = new ThreadWorker (info, server, host, port, listener);
#endif
    threadpool.push_work (w, w, true);
}

#ifdef HAVE_GNUTLS
void
SocketCreator :: on_verify_cert_failed(gnutls_x509_crt_t cert, std::string server, int nr)
{}

void
SocketCreator :: on_valid_cert_added (gnutls_x509_crt_t cert, std::string server)
{}
#endif
