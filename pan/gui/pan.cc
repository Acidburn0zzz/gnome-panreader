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
#include <signal.h>
extern "C" {
  #include <glib/gi18n.h>
  #include <gtk/gtk.h>
  #include <gmime/gmime.h>
}
#include <pan/general/debug.h>
#include <pan/general/log.h>
#include <pan/general/file-util.h>
#include <pan/tasks/socket-impl-gio.h>
#include <pan/tasks/task-groups.h>
#include <pan/tasks/nzb.h>
#include <pan/data-impl/data-impl.h>
#include <pan/icons/pan-pixbufs.h>
#include "gui.h"
#include "group-prefs.h"
#include "prefs-file.h"
#include "task-pane.h"
#include "server-ui.h"
#include "pad.h"

using namespace pan;

namespace
{
  GMainLoop * nongui_gmainloop (0);

  void mainloop ()
  {
#if 1
    if (nongui_gmainloop)
      g_main_loop_run (nongui_gmainloop);
    else
      gtk_main ();
#else
    while (gtk_events_pending ())
      gtk_main_iteration ();
#endif
  }

  void mainloop_quit ()
  {
    if (nongui_gmainloop)
      g_main_loop_quit (nongui_gmainloop);
    else
      gtk_main_quit ();
  }

  gboolean delete_event_cb (GtkWidget *w, GdkEvent *e, gpointer user_data)
  {
    mainloop_quit ();
    return true; // don't invoke the default handler that destroys the widget
  }

#ifndef G_OS_WIN32
  void sighandler (int signum)
  {
    std::cerr << "shutting down pan." << std::endl;
    signal (signum, SIG_DFL);
    mainloop_quit ();
  }
#endif // G_OS_WIN32

  void register_shutdown_signals ()
  {
#ifndef G_OS_WIN32
    signal (SIGHUP, sighandler);
    signal (SIGINT, sighandler);
    signal (SIGTERM, sighandler);
#endif // G_OS_WIN32
  }

  void destroy_cb (GtkWidget*w, gpointer user_data)
  {
    gtk_main_quit ();
  }

  struct DataAndQueue
  {
    Data * data;
    Queue * queue;
  };

  void add_grouplist_task (GtkObject *object, gpointer user_data)
  {
    DataAndQueue * foo (static_cast<DataAndQueue*>(user_data));
    const quarks_t new_servers (foo->data->get_servers());
    foreach_const (quarks_t, new_servers, it)
      foo->queue->add_task (new TaskGroups (*foo->data, *it));
    g_free (foo);
  }

  gboolean queue_upkeep_timer_cb (gpointer queue_gpointer)
  {
    static_cast<Queue*>(queue_gpointer)->upkeep ();
    return true;
  }

  void run_pan_in_window (ArticleCache  & cache,
                          Data          & data,
                          Queue         & queue,
                          Prefs         & prefs,
                          GroupPrefs    & group_prefs,
                          GtkWindow     * window)
  {
    {
      const gulong delete_cb_id =  g_signal_connect (window, "delete-event", G_CALLBACK(delete_event_cb), 0);

      GUI gui (data, queue, cache, prefs, group_prefs);
      gtk_container_add (GTK_CONTAINER(window), gui.root());
      gtk_widget_show (GTK_WIDGET(window));

      const quarks_t servers (data.get_servers ());
      if (servers.empty())
      {
        const Quark empty_server;
        GtkWidget * w = server_edit_dialog_new (data, queue, window, empty_server);
        gtk_widget_show_all (w);
        GtkWidget * msg = gtk_message_dialog_new (GTK_WINDOW(w),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_INFO,
                                                  GTK_BUTTONS_CLOSE,
                                                _("Thank you for trying Pan!\n \nTo start newsreading, first Add a Server."));
        g_signal_connect_swapped (msg, "response", G_CALLBACK (gtk_widget_destroy), msg);
        gtk_widget_show_all (msg);

        DataAndQueue * foo = g_new0 (DataAndQueue, 1);
        foo->data = &data;
        foo->queue = &queue;
        g_signal_connect (w, "destroy", G_CALLBACK(add_grouplist_task), foo);
      }

      register_shutdown_signals ();
      mainloop ();
      g_signal_handler_disconnect (window, delete_cb_id);
    }

    gtk_widget_destroy (GTK_WIDGET(window));
  }

  void usage ()
  {
    std::cerr << "Pan " << VERSION << "\n\n" <<
_("General Options\n"
"  -h, --help               Show this usage page.\n"
"\n"
"URL Options\n"
"  news:message-id          Show the specified article.\n"
"  news:group.name          Show the specified newsgroup.\n"
"  --nogui                  On news:message-id, dump the article to stdout.\n"
"\n"
"NZB Batch Options\n"
"  --nzb file1 file2 ...    Process nzb files without launching all of Pan.\n"
"  -o path, --output=path   Path to save attachments listed in the nzb files.\n"
"  --nogui                  Only show console output, not the download queue.\n") << std::endl;
  }
}

int
main (int argc, char *argv[])
{
  bindtextdomain (GETTEXT_PACKAGE, PANLOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_thread_init (0);
  gtk_init (&argc, &argv);
  g_mime_init (GMIME_INIT_FLAG_UTF8);

  bool gui(true), nzb(false);
  std::string url;
  typedef std::vector<std::string> strings_t;
  strings_t nzb_files;

  char * pch = g_get_current_dir ();
  std::string nzb_output_path = pch;
  g_free (pch);

  for (int i=1; i<argc; ++i)
  {
    const char * tok (argv[i]);
    if (!memcmp(tok,"news:", 5))
      url = tok;
    else if (!strcmp(tok,"--nogui")) 
      gui = false;
    else if (!strcmp (tok, "--debug"))
      _debug_flag = true;
    else if (!strcmp (tok, "--nzb"))
      nzb = true;
    else if (!strcmp (tok, "-o") && i<argc-1) 
      nzb_output_path = argv[++i];
    else if (!memcmp (tok, "--output=", 9))
      nzb_output_path = tok+9;
    else if (!strcmp(tok,"-h") || !strcmp(tok,"--help"))
      { usage (); return 0; }
    else {
      nzb = true;
      nzb_files.push_back (tok);
    }
  }

#if 0
  if (nzb && nzb_files.empty()) {
    std::cerr << _("Error: nzb arguments used without nzb files.") << std::endl;
    return 0;
  }
#endif
  if (!gui && nzb_files.empty() && url.empty()) {
    std::cerr << _("Error: --nogui used without nzb files or news:message-id.") << std::endl;
    return 0;
  }

  Log::add_info_va (_("Pan %s started"), VERSION);

  {
    // load the preferences...
    char * filename = g_build_filename (file::get_pan_home().c_str(), "preferences.xml", NULL);
    PrefsFile prefs (filename);
    g_free (filename);
    filename = g_build_filename (file::get_pan_home().c_str(), "group-preferences.xml", NULL);
    GroupPrefs group_prefs (filename);
    g_free (filename);

    // instantiate the backend...
    const int cache_megs = prefs.get_int ("cache-size-megs", 10);
    DataImpl data (false, cache_megs);
    ArticleCache& cache (data.get_cache ());
    if (nzb && data.get_servers().empty()) {
      std::cerr << _("Please configure Pan's news servers before using it as an nzb client.") << std::endl;
       return 0;
    }

    // instantiate the queue...
    GIOChannelSocket::Creator socket_creator;
    Queue queue (data, data, &socket_creator, prefs.get_flag("work-online",true));
    g_timeout_add (5000, queue_upkeep_timer_cb, &queue);

    if (nzb)
    {
      // load the nzb files...
      std::vector<Task*> tasks;
      foreach_const (strings_t, nzb_files, it)
        NZB :: tasks_from_nzb_file (*it, nzb_output_path, cache, data, data, data, tasks);
      queue.add_tasks (tasks, Queue::BOTTOM);

      // don't open the full-blown Pan, just act as a nzb client,
      // with a gui or without.
      if (gui) {
        TaskPane * pane = new TaskPane (queue, prefs);
        GtkWidget * w (pane->root());
        gtk_widget_show_all (w);
        g_signal_connect (w, "destroy", G_CALLBACK(destroy_cb), 0);
        g_signal_connect (G_OBJECT(w), "delete-event", G_CALLBACK(delete_event_cb), 0);
      } else {
        nongui_gmainloop = g_main_loop_new (NULL, false);
      }
      register_shutdown_signals ();
      mainloop ();
    }
    else
    {
      GdkPixbuf * pixbuf = gdk_pixbuf_new_from_inline (-1, icon_pan, FALSE, 0);
      GtkWidget * window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      if (prefs.get_flag ("main-window-is-maximized", false))
        gtk_window_maximize (GTK_WINDOW (window));
      else
        gtk_window_unmaximize (GTK_WINDOW (window));
      gtk_window_set_role (GTK_WINDOW(window), "pan-main-window");
      prefs.set_window ("main-window", GTK_WINDOW(window), 100, 100, 900, 700);
      gtk_window_set_title (GTK_WINDOW(window), "Pan");
      gtk_window_set_resizable (GTK_WINDOW(window), true);
      gtk_window_set_icon (GTK_WINDOW(window), pixbuf);
      g_object_unref (pixbuf);
      run_pan_in_window (cache, data, queue, prefs, group_prefs, GTK_WINDOW(window));
    }
  }

  g_mime_shutdown ();
  Quark::dump (std::cerr);
  return 0;
}