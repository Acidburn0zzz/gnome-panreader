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
#ifndef _Pan_h_
#define _Pan_h_

#include <pan/general/log.h>
#include <pan/general/progress.h>
#include <pan/data/article-cache.h>
#include <pan/tasks/queue.h>

#include <pan/gui/action-manager.h>
#include <pan/gui/pan-ui.h>
#include <pan/gui/prefs.h>
#include <pan/gui/group-prefs.h>

namespace pan
{
  class GroupPane;
  class HeaderPane;
  class BodyPane;
  class ProgressView;

  /**
   * The main GUI object for Pan's GTK frontend
   * @ingroup GUI
   */
  struct GUI:
    public virtual PanUI,
    public ActionManager,
    private Log::Listener,
    private Progress::Listener,
    private Queue::Listener,
    private Prefs::Listener
  {
    public:
      GUI (Data& data, Queue&, ArticleCache&, Prefs&, GroupPrefs&);
      virtual ~GUI ();
      GtkWidget* root () { return _root; }

    public: // ActionManager
      virtual bool is_action_active (const char * action_name) const;
      virtual void activate_action (const char * action_name) const;
      virtual void toggle_action (const char * action_name, bool) const;
      virtual void sensitize_action (const char * action_name, bool) const;
      virtual GtkWidget* get_action_widget (const char * key) const;
      virtual void disable_accelerators_when_focused (GtkWidget * entry) const;

    public: // Prefs::Listener
      virtual void on_prefs_flag_changed (const StringView& key, bool value);
      virtual void on_prefs_int_changed (const StringView& key, int value) { }
      virtual void on_prefs_string_changed (const StringView& key, const StringView& value);
      virtual void on_prefs_color_changed (const StringView& key, const GdkColor& value) { }

    public: // PanUI
      virtual void do_save_articles ();
      virtual void do_print ();
      virtual void do_quit ();
      virtual void do_import_tasks ();
      virtual void do_cancel_latest_task ();
      virtual void do_show_task_window ();
      virtual void do_show_log_window ();
      virtual void do_select_all_articles ();
      virtual void do_unselect_all_articles ();
      virtual void do_add_similar_to_selection ();
      virtual void do_add_threads_to_selection ();
      virtual void do_add_subthreads_to_selection ();
      virtual void do_select_article_body ();
      virtual void do_show_preferences_dialog ();
      virtual void do_show_group_preferences_dialog ();
      virtual void do_show_profiles_dialog ();
      virtual void do_jump_to_group_tab ();
      virtual void do_jump_to_header_tab ();
      virtual void do_jump_to_body_tab ();
      virtual void do_rot13_selected_text ();
      virtual void do_download_selected_article ();
      virtual void do_clear_header_pane ();
      virtual void do_clear_body_pane ();
      virtual void do_read_selected_article ();
      virtual void do_read_more ();
      virtual void do_read_less ();
      virtual void do_read_next_unread_group ();
      virtual void do_read_next_group ();
      virtual void do_read_next_unread_article ();
      virtual void do_read_next_article ();
      virtual void do_read_next_watched_article ();
      virtual void do_read_next_unread_thread ();
      virtual void do_read_next_thread ();
      virtual void do_read_previous_article ();
      virtual void do_read_previous_thread ();
      virtual void do_read_parent_article ();
      virtual void do_show_servers_dialog ();
      virtual void do_plonk ();
      virtual void do_watch ();
      virtual void do_ignore ();
      virtual void do_show_score_dialog ();
      virtual void do_show_new_score_dialog ();
      virtual void do_cancel_article ();
      virtual void do_supersede_article ();
      virtual void do_delete_article ();
      virtual void do_mark_article_read ();
      virtual void do_mark_article_unread ();
      virtual void do_post ();
      virtual void do_followup_to ();
      virtual void do_reply_to ();
      virtual void do_pan_web ();
      virtual void do_bug_report ();
      virtual void do_tip_jar ();
      virtual void do_about_pan ();
      virtual void do_work_online (bool);
      virtual void do_tabbed_layout (bool);
      virtual void do_show_toolbar (bool);
      virtual void do_show_group_pane (bool);
      virtual void do_show_header_pane (bool);
      virtual void do_show_body_pane (bool);
      virtual void do_shorten_group_names (bool);
      virtual void do_match_only_cached_articles (bool);
      virtual void do_match_only_binary_articles (bool);
      virtual void do_match_only_my_articles (bool);
      virtual void do_match_only_unread_articles (bool);
      virtual void do_match_on_score_state (int);
      virtual void do_show_matches (const Data::ShowType);
      virtual void do_read_selected_group ();
      virtual void do_mark_selected_groups_read ();
      virtual void do_clear_selected_groups ();
      virtual void do_xover_selected_groups ();
      virtual void do_xover_subscribed_groups ();
      virtual void do_download_headers ();
      virtual void do_refresh_groups ();
      virtual void do_subscribe_selected_groups ();
      virtual void do_unsubscribe_selected_groups ();
      virtual void do_set_charset (const char *);


    private: // Queue::Listener
      friend class Queue;
      virtual void on_queue_task_active_changed (Queue&, Task&, bool active);
      virtual void on_queue_tasks_added (Queue&, int index, int count) { }
      virtual void on_queue_task_removed (Queue&, Task&, int pos) { }
      virtual void on_queue_task_moved (Queue&, Task&, int new_pos, int old_pos) { }
      virtual void on_queue_connection_count_changed (Queue&, int count);
      virtual void on_queue_size_changed (Queue&, int active, int total);
      virtual void on_queue_online_changed (Queue&, bool online);
      virtual void on_queue_error (Queue&, const std::string& message);


    private: // Log::Listener
      virtual void on_log_entry_added (const Log::Entry& e);
      virtual void on_log_cleared () {}

    private: // Progress::Listener
      Article latest_read_article;
      virtual void on_progress_finished (Progress&, int status);

    private:
      void set_selected_thread_score (int score);

    private:
      void watch_cursor_on ();
      void watch_cursor_off ();

    private:
      Data& _data;
      Queue& _queue;
      ArticleCache& _cache;
      Prefs& _prefs;
      GroupPrefs& _group_prefs;

    private:
      GtkWidget * _root;
      GtkWidget * _menu_vbox;
      GtkWidget * _workarea_bin;
      GtkWidget * _toolbar;
      GroupPane * _group_pane;
      HeaderPane * _header_pane;
      BodyPane * _body_pane;
      GtkUIManager * _ui_manager;

      GtkWidget * _info_image;
      GtkWidget * _error_image;

      GtkWidget * _connection_size_eventbox;
      GtkWidget * _connection_size_label;
      GtkWidget * _queue_size_label;
      GtkWidget * _queue_size_button;
      GtkWidget * _event_log_button;
      GtkWidget * _taskbar;
      std::vector<ProgressView*> _views;
      std::list<Task*> _active_tasks;
      GtkTooltips * _ttips;

      std::string _fallback_charset;

      void upkeep ();
      guint upkeep_tag;
      static int upkeep_timer_cb (gpointer gui_g);
      void set_queue_size_label (unsigned int running, unsigned int size);
      void refresh_connection_label ();

      static void show_event_log_cb (GtkWidget*, gpointer);
      static void show_task_window_cb (GtkWidget*, gpointer);

      void score_add (int);

    private:
      static void add_widget (GtkUIManager*, GtkWidget*, gpointer);
      static void server_list_dialog_destroyed_cb (GtkWidget*, gpointer);
      void server_list_dialog_destroyed (GtkWidget*);
  };
}

#endif /* __PAN_H__ */