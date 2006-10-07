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
#include <map>
#include <string>
extern "C" {
  #include <glib/gi18n.h>
}
#include <pan/general/debug.h>
#include <pan/general/file-util.h>
#include <pan/general/foreach.h>
#include <pan/data/scorefile.h>
#include <pan/tasks/task-article.h>
#include <pan/tasks/task-groups.h>
#include <pan/tasks/task-xover.h>
#include <pan/tasks/nzb.h>
#include <pan/icons/pan-pixbufs.h>
#include "actions.h"
#include "body-pane.h"
#include "dl-headers-ui.h"
#include "group-pane.h"
#include "group-prefs-dialog.h"
#include "header-pane.h"
#include "hig.h"
#include "license.h"
#include "log-ui.h"
#include "gui.h"
#include "pad.h"
#include "pan.ui.h"
#include "prefs-ui.h"
#include "progress-view.h"
#include "profiles-dialog.h"
#include "post-ui.h"
#include "save-ui.h"
#include "score-add-ui.h"
#include "score-view-ui.h"
#include "server-ui.h"
#include "task-pane.h"
#include "url.h"

using namespace pan;

namespace
{
  const int VIEW_QTY = 3;

  GtkWindow* get_window (GtkWidget* w)
  {
    return GTK_WINDOW (gtk_widget_get_toplevel (w));
  }

  void
  parent_set_cb (GtkWidget * widget, GtkObject * old_parent, gpointer ui_manager_g)
  {
    GtkWidget * toplevel = gtk_widget_get_toplevel (widget);
    if (GTK_IS_WINDOW (toplevel))
    {
      GtkUIManager * ui_manager = static_cast<GtkUIManager*>(ui_manager_g);
      gtk_window_add_accel_group (GTK_WINDOW(toplevel),
                                  gtk_ui_manager_get_accel_group (ui_manager));
    }
  }

  void set_visible (GtkWidget * w, bool visible)
  {
    if (visible)
      gtk_widget_show (w);
    else
      gtk_widget_hide (w);
  }
}

void
GUI :: add_widget (GtkUIManager * merge,
                   GtkWidget    * widget,
                   gpointer       gui_g)
{
  const char * name (gtk_widget_get_name (widget));
  GUI * self (static_cast<GUI*>(gui_g));

  if (name && strstr(name,"main-window-")==name)
  {
    if (!GTK_IS_TOOLBAR (widget))
    {
      gtk_box_pack_start (GTK_BOX(self->_menu_vbox), widget, FALSE, FALSE, 0);
      gtk_widget_show (widget);
    }
    else
    {
      gtk_toolbar_set_style (GTK_TOOLBAR(widget), GTK_TOOLBAR_ICONS);
      gtk_toolbar_set_icon_size (GTK_TOOLBAR(widget), GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_box_pack_start (GTK_BOX(self->_menu_vbox), widget, FALSE, FALSE, 0);
      set_visible (widget, self->is_action_active("show-toolbar"));
      self->_toolbar = widget;
    }
  }
}

void GUI :: show_event_log_cb (GtkWidget * w, gpointer gui_gpointer)
{
  static_cast<GUI*>(gui_gpointer)->activate_action ("show-log-dialog");
}

void GUI :: show_task_window_cb (GtkWidget * w, gpointer gui_gpointer)
{
  static_cast<GUI*>(gui_gpointer)->activate_action ("show-task-window");
}

namespace
{
  std::string get_accel_filename () {
    char * tmp = g_build_filename (file::get_pan_home().c_str(), "accels.txt", NULL);
    std::string ret (tmp);
    g_free (tmp);
    return ret;
  }
}


GUI :: GUI (Data& data, Queue& queue, ArticleCache& cache, Prefs& prefs, GroupPrefs& group_prefs):
  _data (data),
  _queue (queue),
  _cache (cache),
  _prefs (prefs),
  _group_prefs (group_prefs),
  _root (gtk_vbox_new (FALSE, 0)),
  _menu_vbox (gtk_vbox_new (FALSE, 0)),
  _group_pane (0),
  _header_pane (0),
  _body_pane (0),
  _ui_manager (gtk_ui_manager_new ()),
  _info_image (gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_BUTTON)),
  _error_image (gtk_image_new_from_stock (GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_BUTTON)),
  _connection_size_eventbox (0),
  _connection_size_label (0),
  _queue_size_label (0),
  _queue_size_button (0),
  _taskbar (0),
  _ttips (gtk_tooltips_new ())
{
  char * filename = g_build_filename (file::get_pan_home().c_str(), "pan.ui", NULL);
#if 0
  GError * gerr (0);
  gtk_ui_manager_add_ui_from_file (_ui_manager, filename, &gerr);
  if (gerr)  {
    Log::add_err (gerr->message);
    g_clear_error (&gerr);
  }
#else
  if (!gtk_ui_manager_add_ui_from_file (_ui_manager, filename, NULL))
    gtk_ui_manager_add_ui_from_string (_ui_manager, fallback_ui_file, -1, NULL);
#endif
  g_free (filename);
  g_signal_connect (_ui_manager, "add_widget", G_CALLBACK (add_widget), this);
  add_actions (this, _ui_manager, &prefs);
  g_signal_connect (_root, "parent-set", G_CALLBACK(parent_set_cb), _ui_manager);
  gtk_box_pack_start (GTK_BOX(_root), _menu_vbox, FALSE, FALSE, 0);
  gtk_widget_show (_menu_vbox);

  //_group_pane = new GroupPane (*this, data, _prefs);
  _group_pane = new GroupPane (*this, data, _prefs);
  _header_pane = new HeaderPane (*this, data, _queue, _cache, _prefs);
  _body_pane = new BodyPane (data, _cache, _prefs);

  std::string path = "/ui/main-window-toolbar";
  GtkWidget * toolbar = gtk_ui_manager_get_widget (_ui_manager, path.c_str());
  path += "/group-pane-filter";
  GtkWidget * w = gtk_ui_manager_get_widget (_ui_manager, path.c_str());
  int index = gtk_toolbar_get_item_index (GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(w));
  GtkToolItem * item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER(item), _group_pane->create_filter_entry());
  gtk_widget_show_all (GTK_WIDGET(item));
  gtk_toolbar_insert (GTK_TOOLBAR(toolbar), item, index+1);
  path = "/ui/main-window-toolbar/header-pane-filter";
  w = gtk_ui_manager_get_widget (_ui_manager, path.c_str());
  index = gtk_toolbar_get_item_index (GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(w));
  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER(item), _header_pane->create_filter_entry());
  gtk_widget_show_all (GTK_WIDGET(item));
  gtk_toolbar_insert (GTK_TOOLBAR(toolbar), item, index+1);
  
  //guint merge_id = gtk_ui_manager_new_merge_id (_ui_manager);
  //gtk_ui_manager_add_ui (_ui_manager, merge_id, path, "group-pane-filter", NULL, GTK_UI_MANAGER_TOOLITEM, true);
  //GtkWidget * item = gtk_ui_manager_get_widget (_ui_manager, path);
  //gtk_container_add (GTK_CONTAINER(item), _group_pane->create_filter_entry());


  // workarea
  _workarea_bin = gtk_event_box_new ();
  do_tabbed_layout (is_action_active ("tabbed-layout"));
  gtk_box_pack_start (GTK_BOX(_root), _workarea_bin, TRUE, TRUE, 0);
  gtk_widget_show (_workarea_bin);

  /**
  ***  Status Bar
  **/

  w = gtk_event_box_new ();
  gtk_widget_set_size_request (w, -1, PAD_SMALL);
  gtk_box_pack_start (GTK_BOX(_root), w, false, false, 0);
  gtk_widget_show (w);

  GtkWidget * status_bar (gtk_hbox_new (FALSE, PAD_SMALL));

  // connection status
  w = _connection_size_label = gtk_label_new (NULL);
  gtk_misc_set_padding (GTK_MISC(w), PAD, 0);
  _connection_size_eventbox = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER(_connection_size_eventbox), w);
  w = _connection_size_eventbox;
  GtkWidget * frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER(frame), w);
  gtk_box_pack_start (GTK_BOX(status_bar), frame, FALSE, FALSE, 0);

  // queue
  _queue_size_label = gtk_label_new (NULL);
  w = _queue_size_button = gtk_button_new();
  gtk_tooltips_set_tip (GTK_TOOLTIPS(_ttips), w, _("Open the Task Manager"), NULL);
  gtk_button_set_relief (GTK_BUTTON(w), GTK_RELIEF_NONE);
  g_signal_connect (GTK_OBJECT(w), "clicked", G_CALLBACK(show_task_window_cb), this);
  gtk_container_add (GTK_CONTAINER(w), _queue_size_label);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER(frame), w);
  gtk_box_pack_start (GTK_BOX(status_bar), frame, FALSE, FALSE, 0);

  // status item views
  _taskbar = gtk_table_new (1, VIEW_QTY, TRUE);
  for (int i=0; i<VIEW_QTY; ++i) {
    ProgressView * v = new ProgressView ();
    gtk_table_attach (GTK_TABLE(_taskbar), v->root(), i, i+1, 0, 1, (GtkAttachOptions)~0, (GtkAttachOptions)~0, 0, 0);
    _views.push_back (v);
  }
  gtk_box_pack_start (GTK_BOX(status_bar), _taskbar, true, true, 0);
  gtk_widget_show_all (status_bar);

  // status 
  w = _event_log_button = gtk_button_new ();
  gtk_tooltips_set_tip (GTK_TOOLTIPS(_ttips), w, _("Open the Event Log"), NULL);
  gtk_button_set_relief (GTK_BUTTON(w), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX(status_bar), w, false, false, 0);
  gtk_container_add (GTK_CONTAINER(w), _info_image);
  g_signal_connect (w, "clicked", G_CALLBACK(show_event_log_cb), this);

  gtk_box_pack_start (GTK_BOX(_root), status_bar, false, false, 0);
  gtk_widget_show_all (status_bar);

  gtk_widget_show (_root);

  upkeep_tag = g_timeout_add (3000, upkeep_timer_cb, this);

  _queue.add_listener (this);
  Log::get().add_listener (this);

  gtk_widget_ref (_info_image);
  gtk_widget_ref (_error_image);
  gtk_widget_ref (_group_pane->root());
  gtk_widget_ref (_header_pane->root());
  gtk_widget_ref (_body_pane->root());

  gtk_object_sink (GTK_OBJECT (_info_image));
  gtk_object_sink (GTK_OBJECT (_error_image));

  do_work_online (is_action_active ("work-online"));

  // update the connections label
  upkeep_timer_cb (this);

  // update the queue label
  int active(0), total(0);
  _queue.get_task_counts (active, total);
  set_queue_size_label (active, total);

  if (_prefs.get_flag ("get-new-headers-on-startup", false))
    activate_action ("get-new-headers-in-subscribed-groups");

  _prefs.add_listener (this);

  g_object_ref (_ttips);
  gtk_object_sink (GTK_OBJECT(_ttips));
  g_signal_connect_swapped (_root, "destroy", G_CALLBACK(g_object_unref), _ttips);

  gtk_accel_map_load (get_accel_filename().c_str());
}

namespace
{
  GtkWidget * hpane (0);
  GtkWidget * vpane (0);
}

GUI :: ~GUI ()
{
  gtk_accel_map_save (get_accel_filename().c_str());

  if (hpane)
    _prefs.set_int ("main-window-hpane-position", gtk_paned_get_position(GTK_PANED(hpane)));
  if (vpane)
    _prefs.set_int ("main-window-vpane-position", gtk_paned_get_position(GTK_PANED(vpane)));

  const bool maximized = GTK_WIDGET(_root)->window
                      && (gdk_window_get_state(_root->window) & GDK_WINDOW_STATE_MAXIMIZED);
  _prefs.set_flag ("main-window-is-maximized", maximized);

  _prefs.remove_listener (this);
  _queue.remove_listener (this);
  Log::get().remove_listener (this);
  g_source_remove (upkeep_tag);

  std::set<GtkWidget*> unref;
  unref.insert (_body_pane->root());
  unref.insert (_header_pane->root());
  unref.insert (_group_pane->root());
  unref.insert (_error_image);
  unref.insert (_info_image);

  delete _header_pane;
  delete _group_pane;
  delete _body_pane;
  for (size_t i(0), size(_views.size()); i!=size; ++i)
    delete _views[i];

  foreach (std::set<GtkWidget*>, unref, it)
    gtk_widget_unref (*it);
}

/***
****
***/

void
GUI :: watch_cursor_on ()
{
  GdkCursor * cursor = gdk_cursor_new (GDK_WATCH);
  gdk_window_set_cursor (_root->window, cursor);
  gdk_cursor_unref (cursor);
  while (gtk_events_pending ())
    gtk_main_iteration ();
}

void
GUI :: watch_cursor_off ()
{
  gdk_window_set_cursor (_root->window, NULL);
}

/***
****
***/

namespace
{
  typedef std::map < std::string, GtkAction* > key_to_action_t;

  key_to_action_t key_to_action;

  void ensure_action_map_loaded (GtkUIManager * uim)
  {
    if (!key_to_action.empty())
      return;

    for (GList * l=gtk_ui_manager_get_action_groups(uim); l!=0; l=l->next)
    {
      GtkActionGroup * action_group = GTK_ACTION_GROUP(l->data);
      GList * actions = gtk_action_group_list_actions (action_group);
      for (GList * ait(actions); ait; ait=ait->next) {
        GtkAction * action = GTK_ACTION(ait->data);
        const std::string name (gtk_action_get_name (action));
        key_to_action[name] = action;
      }
      g_list_free (actions);
    }
  }

  GtkAction * get_action (const char * name) {
    key_to_action_t::iterator it = key_to_action.find (name);
    if (it == key_to_action.end()) {
      std::cerr << LINE_ID << " can't find action " << name << std::endl;
      abort ();
    }
    return it->second;
  }
}

bool
GUI :: is_action_active (const char *key) const
{
  ensure_action_map_loaded (_ui_manager);
  return gtk_toggle_action_get_active (GTK_TOGGLE_ACTION(get_action(key)));
}

void
GUI :: activate_action (const char * key) const
{
  ensure_action_map_loaded (_ui_manager);
  gtk_action_activate (get_action(key));
}

void
GUI :: sensitize_action (const char * key, bool b) const
{
  ensure_action_map_loaded (_ui_manager);
  g_object_set (get_action(key), "sensitive", gboolean(b), NULL);
  //gtk_action_set_sensitive (get_action(key), b);
}


void
GUI :: toggle_action (const char * key, bool b) const
{
  ensure_action_map_loaded (_ui_manager);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION(get_action(key)), b);
}

GtkWidget*
GUI :: get_action_widget (const char * key) const
{
  return gtk_ui_manager_get_widget (_ui_manager, key);
}

namespace
{
  gboolean focus_in_cb (GtkWidget     * w,
                        GdkEventFocus * unused,
                        gpointer        accel_group_g)
  {
    GtkAccelGroup * accel_group = static_cast<GtkAccelGroup*>(accel_group_g);
    gtk_window_remove_accel_group (get_window(w), accel_group);
    return false;
  }

  gboolean focus_out_cb (GtkWidget     * w,
                         GdkEventFocus * unused,
                         gpointer        accel_group_g)
  {
    GtkAccelGroup * accel_group = static_cast<GtkAccelGroup*>(accel_group_g);
    gtk_window_add_accel_group (get_window(w), accel_group);
    return false;
  }
}

void
GUI :: disable_accelerators_when_focused (GtkWidget * w) const
{
  GtkAccelGroup * accel_group = gtk_ui_manager_get_accel_group (_ui_manager);
  g_signal_connect (w, "focus-in-event", G_CALLBACK(focus_in_cb), accel_group);
  g_signal_connect (w, "focus-out-event", G_CALLBACK(focus_out_cb), accel_group);
}

/***
****  PanUI
***/

namespace
{
  static std::string prev_path;

  std::string get_save_attachments_path_from_user (GtkWindow * parent, const Prefs& prefs)
  {
    if (prev_path.empty())
      prev_path = prefs.get_string ("default-save-attachments-path", g_get_home_dir ());

    GtkWidget * w = gtk_file_chooser_dialog_new (_("Save Attachments"), parent,
                                                 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                 GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                 NULL);
    gtk_dialog_set_default_response (GTK_DIALOG(w), GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(w), prev_path.c_str());
    const int response (gtk_dialog_run (GTK_DIALOG(w)));
    std::string path;
    if (response == GTK_RESPONSE_ACCEPT) {
      char * tmp = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (w));
      prev_path = path = tmp;
      g_free (tmp);
    }

    gtk_widget_destroy(w);
    return path;
  }
}

void GUI :: do_save_articles ()
{
  std::string path;
  const std::vector<const Article*> articles (_header_pane->get_full_selection_v ());

  std::vector<Article> copies;
  copies.reserve (articles.size());
  foreach_const (std::vector<const Article*>, articles, it)
    copies.push_back (**it);

  if (!copies.empty()) {
    SaveDialog * dialog = new SaveDialog (_prefs, _group_prefs, _data, _data, _cache, _data, _queue, get_window(_root), _header_pane->get_group(), copies);
    gtk_widget_show (dialog->root());
  }
}

void GUI :: do_print ()
{
  std::cerr << "FIXME " << LINE_ID << std::endl;
}
void GUI :: do_cancel_latest_task ()
{
  _queue.remove_latest_task ();
}
void GUI :: do_import_tasks ()
{
  // get a list of files to import
  GtkWidget * dialog = gtk_file_chooser_dialog_new (
    _("Import NZB File(s)"), get_window(_root),
    GTK_FILE_CHOOSER_ACTION_OPEN,
    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
    NULL);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER(dialog), true);
  typedef std::vector<std::string> strings_t;
  strings_t filenames;
  if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    GSList * tmp = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(dialog));
    for (GSList * l(tmp); l!=0; l=l->next) {
      filenames.push_back ((char*) l->data);
      g_free (l->data);
    }
    g_slist_free (tmp);
  }
  gtk_widget_destroy (dialog);

  // if we're importing files, build the tasks...
  std::vector<Task*> tasks;
  if (!filenames.empty()) {
    const std::string path (get_save_attachments_path_from_user (get_window(_root), _prefs));
    if (!path.empty())
      foreach_const (strings_t, filenames, it)
        NZB :: tasks_from_nzb_file (*it, path, _cache, _data, _data, _data, tasks);
  }

  if (!tasks.empty())
    _queue.add_tasks (tasks, Queue::BOTTOM);
}

namespace
{
  void task_pane_destroyed_cb (GtkWidget * w, gpointer p)
  {
    TaskPane ** task_pane (static_cast<TaskPane**>(p));
    *task_pane = 0;
  }
}

void GUI :: do_show_task_window ()
{
  static TaskPane * task_pane (0);
  if (task_pane)
    gtk_window_present (GTK_WINDOW(task_pane->root()));
  else {
    task_pane = new TaskPane (_queue, _prefs);
    g_signal_connect (task_pane->root(), "destroy", G_CALLBACK(task_pane_destroyed_cb), &task_pane);
    gtk_widget_show (task_pane->root());
  }
}

namespace
{
  void set_bin_child (GtkWidget * w, GtkWidget * new_child)
  {
    GtkWidget * child (gtk_bin_get_child (GTK_BIN(w)));
    if (child != new_child)
    {
      gtk_container_remove (GTK_CONTAINER(w), child);
      gtk_container_add (GTK_CONTAINER(w), new_child);
      gtk_widget_show (new_child);
    }
  } 
}

void GUI :: on_log_entry_added (const Log::Entry& e)
{
  if (e.severity & Log::PAN_SEVERITY_ERROR)
    set_bin_child (_event_log_button, _error_image);

  if (e.severity & Log::PAN_SEVERITY_URGENT) {
    std::string msg (e.message);
    if (msg.find ("ENOSPC") != e.message.npos) {
      msg.erase (msg.find ("ENOSPC"), 6);
      msg += ' ';
      msg += _("Pan is now offline. Please ensure that space is available, then use File|Work Online to continue.");
      toggle_action ("work-online", false);
    }
    GtkWidget * w = gtk_message_dialog_new (get_window(_root),
                                            GtkDialogFlags(GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT),
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            "%s", msg.c_str());
    g_signal_connect_swapped (w, "response", G_CALLBACK (gtk_widget_destroy), w);
    gtk_widget_show_all (w);
  }
}

void GUI :: do_show_log_window ()
{
  static GtkWidget * log_window (0);
  if (!log_window) {
    log_window = log_dialog_new (_prefs, get_window (_root));
    g_signal_connect (log_window, "destroy", G_CALLBACK(gtk_widget_destroyed), &log_window);
  }
  gtk_window_present (GTK_WINDOW(log_window));
  set_bin_child (_event_log_button, _info_image);
}
void GUI :: do_select_all_articles ()
{
  _header_pane->select_all ();
}
void GUI :: do_unselect_all_articles ()
{
  _header_pane->unselect_all ();
}
void GUI :: do_add_threads_to_selection ()
{
  _header_pane->select_threads ();
}
void GUI :: do_add_similar_to_selection ()
{
  _header_pane->select_similar ();
}
void GUI :: do_add_subthreads_to_selection ()
{
  _header_pane->select_subthreads ();
}
void GUI :: do_select_article_body ()
{
  _body_pane->select_all ();
}
void GUI :: do_show_preferences_dialog ()
{
  PrefsDialog * dialog = new PrefsDialog (_prefs, get_window(_root));
  gtk_widget_show (dialog->root());
}
void GUI :: do_show_group_preferences_dialog ()
{
  const Quark group (_group_pane->get_selection ());
  if (!group.empty())
  {
    GroupPrefsDialog * dialog = new GroupPrefsDialog (_data, group, _group_prefs, get_window(_root));
    gtk_widget_show (dialog->root());
  }
}

void GUI :: do_show_profiles_dialog ()
{
  ProfilesDialog d (_data, _data, get_window(_root));
  gtk_dialog_run (GTK_DIALOG(d.root()));
  gtk_widget_destroy (d.root());
}

void GUI :: do_jump_to_group_tab ()
{
  toggle_action ("tabbed-layout", true);
  GtkNotebook * n (GTK_NOTEBOOK (GTK_BIN (_workarea_bin)->child));
  gtk_notebook_set_current_page (n, 0);
}

void GUI :: do_jump_to_header_tab ()
{
  toggle_action ("tabbed-layout", true);
  GtkNotebook * n (GTK_NOTEBOOK (GTK_BIN (_workarea_bin)->child));
  gtk_notebook_set_current_page (n, 1);
}

void GUI :: do_jump_to_body_tab ()
{
  toggle_action ("tabbed-layout", true);
  GtkNotebook * n (GTK_NOTEBOOK (GTK_BIN (_workarea_bin)->child));
  gtk_notebook_set_current_page (n, 2);
}

void GUI :: do_rot13_selected_text ()
{
  _body_pane->rot13_selected_text ();
}


void GUI :: on_progress_finished (Progress & p, int status)
{
  TaskArticle * ta = dynamic_cast<TaskArticle*>(&p);
  if (status==OK && ta && ta->get_article().message_id == latest_read_article.message_id)
    _body_pane->set_article (latest_read_article);
}

void GUI :: do_read_selected_article ()
{
  const Article* article (_header_pane->get_first_selected_article ());
  if (article)
  {
    latest_read_article = *article;
    Task * t = new TaskArticle (_data, _data, *article, _cache, _data, this);
    _queue.add_task (t, Queue::TOP);

    // expand its thread in the header pane
    _header_pane->expand_selected ();

    // maybe update the tab
    if (is_action_active ("tabbed-layout"))
      activate_action ("jump-to-body-tab");
  }
}
void GUI :: do_download_selected_article ()
{
  typedef std::set<const Article*> article_set_t;
  const article_set_t articles (_header_pane->get_full_selection ());
  foreach_const (article_set_t, articles, it)
    _queue.add_task (new TaskArticle (_data, _data, **it, _cache, _data, this));
}
void GUI :: do_clear_header_pane ()
{
  gtk_window_set_title (get_window(_root), _("Pan"));
  _header_pane->set_group (Quark());
}
void GUI :: do_clear_body_pane ()
{
  _body_pane->clear ();
}
void GUI :: do_read_more ()
{
  if (!_body_pane->read_more ())
  {
    activate_action (_prefs.get_flag ("space-selects-next-article", true)
                     ? "read-next-article"
                     : "read-next-unread-article");
  }
}
void GUI :: do_read_less ()
{
  if (!_body_pane->read_less ())
    activate_action ("read-previous-article");
}
void GUI :: do_read_next_unread_group ()
{
  _group_pane->read_next_unread_group ();
}
void GUI :: do_read_next_group ()
{
  _group_pane->read_next_group ();
}
void GUI :: do_read_next_unread_article ()
{
  _header_pane->read_next_unread_article ();
}
void GUI :: do_read_next_article ()
{
  _header_pane->read_next_article ();
}
void GUI :: do_read_next_watched_article ()
{
  std::cerr << "FIXME " << LINE_ID << std::endl;
}
void GUI :: do_read_next_unread_thread ()
{
  _header_pane->read_next_unread_thread ();
}
void GUI :: do_read_next_thread ()
{
  _header_pane->read_next_thread ();
}
void GUI :: do_read_previous_article ()
{
  _header_pane->read_previous_article ();
}
void GUI :: do_read_previous_thread ()
{
  _header_pane->read_previous_thread ();
}
void GUI :: do_read_parent_article ()
{
  _header_pane->read_parent_article ();
}

void GUI ::  server_list_dialog_destroyed_cb (GtkWidget * w, gpointer self)
{
  static_cast<GUI*>(self)->server_list_dialog_destroyed (w);
}

// this queues up a grouplist task for any servers that
// were added while the server list dialog was up.
void GUI :: server_list_dialog_destroyed (GtkWidget * w)
{
  quarks_t empty_servers, all_servers (_data.get_servers());
  foreach_const (quarks_t, all_servers, it) {
    quarks_t tmp;
    _data.server_get_groups (*it, tmp);
    if (tmp.empty())
      _queue.add_task (new TaskGroups (_data, *it));
  }
}

void GUI :: do_show_servers_dialog ()
{
  GtkWidget * w = server_list_dialog_new (_data, _queue, get_window(_root));
  gtk_widget_show_all (w);
  g_signal_connect (w, "destroy", G_CALLBACK(server_list_dialog_destroyed_cb), this);
}

void GUI :: do_show_score_dialog ()
{
  const Quark& group (_header_pane->get_group());
  const Article * article (0);
  if (!group.empty())
    article = _header_pane->get_first_selected_article ();
  if (article) {
    GtkWindow * window (get_window (_root));
    ScoreView * view = new ScoreView (_data, window, group, *article);
    gtk_widget_show (view->root());
  }
}

void GUI :: set_selected_thread_score (int score)
{
  Quark group (_header_pane->get_group());
  const Article* article (_header_pane->get_first_selected_article ());
  std::string references;
  _data.get_article_references (group, article, references);
  StringView v(references), tok;
  v.pop_token (tok);
  if (tok.empty())
    tok = article->message_id.c_str();

  // if this is the article or a descendant...
  Scorefile::AddItem items[2];
  items[0].on = true;
  items[0].negate = false;
  items[0].key = "Message-ID";
  items[0].value = TextMatch::create_regex (tok, TextMatch::IS);
  items[1].on = true;
  items[1].negate = false;
  items[1].key = "References";
  items[1].value = TextMatch::create_regex (tok, TextMatch::CONTAINS);

  _data.add_score (StringView(group), score, true, 31, false, items, 2, true);
}
void GUI :: do_watch ()
{
  set_selected_thread_score (9999);
}
void GUI :: do_ignore ()
{
  set_selected_thread_score (-9999);
}
void GUI :: do_plonk ()
{
  score_add (ScoreAddDialog::PLONK);
}
void GUI :: do_show_new_score_dialog ()
{
  score_add (ScoreAddDialog::ADD);
}
void
GUI :: score_add (int mode)
{
  Quark group (_header_pane->get_group());
  if (group.empty())
    group = _group_pane->get_selection();

  Article a;
  const Article* article (_header_pane->get_first_selected_article ());
  if (article != 0)
    a = *article;

  ScoreAddDialog * d = new ScoreAddDialog (_data, _root, group, a, (ScoreAddDialog::Mode)mode);
  gtk_widget_show (d->root());
}

void GUI :: do_supersede_article ()
{
  GMimeMessage * message (_body_pane->get_message ());
  if (!message)
    return;

  // did this user post the message?
  const char * sender (g_mime_message_get_sender (message));
  const bool user_posted_this (_data.has_from_header (sender));
  if (!user_posted_this) {
    GtkWidget * w = gtk_message_dialog_new (
      get_window(_root),
      GtkDialogFlags(GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT),
      GTK_MESSAGE_ERROR,
      GTK_BUTTONS_CLOSE, NULL);
    HIG :: message_dialog_set_text (GTK_MESSAGE_DIALOG(w),
      _("Unable to supersede article."),
      _("The article doesn't match any of your posting profiles."));
    g_signal_connect_swapped (w, "response", G_CALLBACK (gtk_widget_destroy), w);
    gtk_widget_show (w);
    g_object_unref (message);
    return;
  }

  // copy the body and preserve the essential headers...
  // if we copy the entire message, then we get all the
  // headers tacked on by the news server.
  const char * cpch;
  char * old_mid (g_strdup_printf ("<%s>", g_mime_message_get_message_id(message)));
  GMimeMessage * new_message (g_mime_message_new (false));
  g_mime_message_set_header (new_message, "Supersedes", old_mid);
  g_mime_message_set_sender (new_message, g_mime_message_get_sender (message));
  g_mime_message_set_subject (new_message, g_mime_message_get_subject (message));
  g_mime_message_set_header (new_message, "Newsgroups", g_mime_message_get_header (message, "Newsgroups"));
  if ((cpch = g_mime_message_get_reply_to (message)))
              g_mime_message_set_reply_to (new_message, cpch);
  if ((cpch = g_mime_message_get_header (message,     "Followup-To")))
              g_mime_message_set_header (new_message, "Followup-To", cpch);
  gboolean  unused (false);
  char * body (g_mime_message_get_body (message, true, &unused));
  GMimeStream * stream = g_mime_stream_mem_new_with_buffer (body, strlen(body));
  GMimeDataWrapper * content_object = g_mime_data_wrapper_new_with_stream (stream, GMIME_PART_ENCODING_DEFAULT);
  GMimePart * part = g_mime_part_new ();
  g_mime_part_set_content_object (part, content_object);
  g_mime_message_set_mime_part (new_message, GMIME_OBJECT(part));
  g_object_unref (part);
  g_object_unref (content_object);
  g_object_unref (stream);

  PostUI * post = PostUI :: create_window (get_window(_root), _data, _queue, _data, _data, new_message, _prefs, _group_prefs);
  if (post)
  {
    gtk_widget_show_all (post->root());

    GtkWidget * w = gtk_message_dialog_new (
      GTK_WINDOW(post->root()),
      GtkDialogFlags(GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT),
      GTK_MESSAGE_INFO,
      GTK_BUTTONS_CLOSE, NULL);
    HIG :: message_dialog_set_text (GTK_MESSAGE_DIALOG(w),
      _("Revise and send this article to replace the old one."),
      _("Be patient!  It will take time for your changes to take effect."));
    g_signal_connect_swapped (w, "response", G_CALLBACK (gtk_widget_destroy), w);
    gtk_widget_show (w);
  }

  g_object_unref (message);
  g_free (old_mid);
}

void GUI :: do_cancel_article ()
{
  GMimeMessage * message (_body_pane->get_message ());
  if (!message)
    return;

  // did this user post the message?
  const char * sender (g_mime_message_get_sender (message));
  const bool user_posted_this (_data.has_from_header (sender));
  if (!user_posted_this) {
    GtkWidget * w = gtk_message_dialog_new (
      get_window(_root),
      GtkDialogFlags(GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT),
      GTK_MESSAGE_ERROR,
      GTK_BUTTONS_CLOSE, NULL);
    HIG :: message_dialog_set_text (GTK_MESSAGE_DIALOG(w),
      _("Unable to cancel article."),
      _("The article doesn't match any of your posting profiles."));
    g_signal_connect_swapped (w, "response", G_CALLBACK (gtk_widget_destroy), w);
    gtk_widget_show (w);
    g_object_unref (message);
    return;
  }

  // okay then...
  GMimeMessage * cancel = g_mime_message_new (false);
  char * cancel_message = g_strdup_printf ("cancel <%s>", g_mime_message_get_message_id(message));
  g_mime_message_set_sender (cancel, g_mime_message_get_sender (message));
  g_mime_message_set_subject (cancel, "Cancel");
  g_mime_message_set_header (cancel, "Newsgroups", g_mime_message_get_header (message, "Newsgroups"));
  g_mime_message_set_header (cancel, "Control", cancel_message);
  const char * body ("Ignore\r\nArticle canceled by author using " PACKAGE_STRING "\r\n");
  GMimeStream * stream = g_mime_stream_mem_new_with_buffer (body, strlen(body));
  GMimeDataWrapper * content_object = g_mime_data_wrapper_new_with_stream (stream, GMIME_PART_ENCODING_DEFAULT);
  GMimePart * part = g_mime_part_new ();
  g_mime_part_set_content_object (part, content_object);
  g_mime_message_set_mime_part (cancel, GMIME_OBJECT(part));
  g_object_unref (part);
  g_object_unref (content_object);
  g_object_unref (stream);
  g_free (cancel_message);

  PostUI * post = PostUI :: create_window (get_window(_root), _data, _queue, _data, _data, cancel, _prefs, _group_prefs);
  if (post)
  {
    gtk_widget_show_all (post->root());

    GtkWidget * w = gtk_message_dialog_new (
      GTK_WINDOW(post->root()),
      GtkDialogFlags(GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT),
      GTK_MESSAGE_INFO,
      GTK_BUTTONS_CLOSE, NULL);
    HIG :: message_dialog_set_text (GTK_MESSAGE_DIALOG(w),
      _("Send this article to ask your server to cancel your other one."),
      _("Be patient!  It will take time for your changes to take effect."));
    g_signal_connect_swapped (w, "response", G_CALLBACK (gtk_widget_destroy), w);
    gtk_widget_show (w);
  }

  g_object_unref (message);
}

void GUI :: do_delete_article ()
{
  const std::set<const Article*> articles (_header_pane->get_nested_selection());
  _data.delete_articles (articles);

  const Quark mid (_body_pane->get_message_id());
  foreach_const (std::set<const Article*>, articles, it)
    if ((*it)->message_id == mid)
      _body_pane->clear ();
}
void GUI :: do_mark_article_read ()
{
  const std::set<const Article*> article_set (_header_pane->get_nested_selection ());
  const std::vector<const Article*> tmp (article_set.begin(), article_set.end());
  _data.mark_read ((const Article**)&tmp.front(), tmp.size());
}

void GUI :: do_mark_article_unread ()
{
  const std::set<const Article*> article_set (_header_pane->get_nested_selection ());
  const std::vector<const Article*> tmp (article_set.begin(), article_set.end());
  _data.mark_read ((const Article**)&tmp.front(), tmp.size(), false);
}

void
GUI :: do_post ()
{
  GMimeMessage * message = g_mime_message_new (false);

  // add newsgroup...
  Quark group (_header_pane->get_group ());
  if (group.empty())
    group = _group_pane->get_selection ();
  if (!group.empty())
    g_mime_message_add_header (message, "Newsgroups", group.c_str());

  // content type
  GMimePart * part = g_mime_part_new ();
  g_mime_part_set_content_type (part, g_mime_content_type_new_from_string ("text/plain; charset=UTF-8"));
  g_mime_part_set_encoding (part, GMIME_PART_ENCODING_8BIT);
  g_mime_message_set_mime_part (message, GMIME_OBJECT(part));
  g_object_unref (part);

  PostUI * post = PostUI :: create_window (get_window(_root), _data, _queue, _data, _data, message, _prefs, _group_prefs);
  if (post)
    gtk_widget_show_all (post->root());
  g_object_unref (message);
}

void GUI :: do_followup_to ()
{
  GMimeMessage * message = _body_pane->create_followup_or_reply (false);
  if (message) {
    PostUI * post = PostUI :: create_window(get_window(_root), _data, _queue, _data, _data, message, _prefs, _group_prefs);
    if (post)
      gtk_widget_show_all (post->root());
    g_object_unref (message);
  }
}
void GUI :: do_reply_to ()
{
  GMimeMessage * message = _body_pane->create_followup_or_reply (true);
  if (message) {
    PostUI * post = PostUI :: create_window (get_window(_root), _data, _queue, _data, _data, message, _prefs, _group_prefs);
    if (post)
      gtk_widget_show_all (post->root());
    g_object_unref (message);
  }
}

void GUI :: do_pan_web ()
{
  URL :: open (_prefs, "http://pan.rebelbase.com/");
}
void GUI :: do_bug_report ()
{
  URL :: open (_prefs, "http://bugzilla.gnome.org/enter_bug.cgi?product=Pan");
}
void GUI :: do_tip_jar ()
{
  URL :: open (_prefs, "http://pan.rebelbase.com/tipjar/");
}
void GUI :: do_about_pan ()
{
#if GTK_CHECK_VERSION(2,6,0)
  const gchar * authors [] = { "Charles Kerr", 0 };
  GdkPixbuf * logo = gdk_pixbuf_new_from_inline(-1, icon_pan_about_logo, 0, 0);
  GtkAboutDialog * w (GTK_ABOUT_DIALOG (gtk_about_dialog_new ()));
  gtk_about_dialog_set_name (w, _("Pan"));
  gtk_about_dialog_set_version (w, PACKAGE_VERSION);
  gtk_about_dialog_set_comments (w, VERSION_TITLE);
  gtk_about_dialog_set_copyright (w, _("Copyright © 2002-2006 Charles Kerr"));
  gtk_about_dialog_set_website (w, "http://pan.rebelbase.com/");
  gtk_about_dialog_set_logo (w, logo);
  gtk_about_dialog_set_license (w, LICENSE);
  gtk_about_dialog_set_authors (w, authors);
  gtk_about_dialog_set_translator_credits (w, _("translator-credits"));
  g_signal_connect (G_OBJECT (w), "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_widget_show_all (GTK_WIDGET(w));
  g_object_unref (logo);
#else
  GtkWidget * dialog = gtk_dialog_new_with_buttons (PACKAGE_STRING,
                                                    GTK_WINDOW (get_window (_root)),
                                                    GtkDialogFlags (GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT),
                                                    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                                    NULL);
  GdkPixbuf * logo = gdk_pixbuf_new_from_inline(-1, icon_pan_about_logo, 0, 0);
  GtkBox * box = GTK_BOX(GTK_DIALOG(dialog)->vbox);
  gtk_box_pack_start (box, gtk_image_new_from_pixbuf (logo), false, false, PAD);
  gtk_box_pack_start (box, gtk_label_new("Pan " PACKAGE_VERSION), false, false, PAD);
  gtk_box_pack_start (box, gtk_label_new(VERSION_TITLE), false, false, 0);
  gtk_box_pack_start (box, gtk_label_new(_("Copyright © 2002-2006 Charles Kerr")), false, false, 0);
  gtk_box_pack_start (box, gtk_label_new("http://pan.rebelbase.com/"), false, false, PAD);
  gtk_widget_show_all (dialog);
  g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
  g_object_unref (logo);
#endif
}

void GUI :: do_work_online (bool b)
{
  _queue.set_online (b);
  refresh_connection_label ();
}

namespace
{
  void remove_from_parent (GtkWidget * w)
  {
    if (w->parent != 0)
      gtk_container_remove (GTK_CONTAINER(w->parent), w);
  }

  enum { HORIZONTAL, VERTICAL };

  void hpane_destroy_cb (GtkWidget * w, gpointer user_data)
  {
    std::cerr << LINE_ID << std::endl;
  }
  void vpane_destroy_cb (GtkWidget * w, gpointer user_data)
  {
    std::cerr << LINE_ID << std::endl;
  }

  GtkWidget* pack_widgets (Prefs& prefs, GtkWidget * w1, GtkWidget * w2, int orient, gint uglyhack_idx)
  {
    GtkWidget * w;
    if (w1!=NULL && w2!=NULL) {
      int pos = uglyhack_idx==0
        ? prefs.get_int ("main-window-vpane-position", 300)
        : prefs.get_int ("main-window-hpane-position", 266);
      if (orient == VERTICAL) {
        w = vpane = gtk_vpaned_new ();
        gtk_widget_set_size_request (w1, -1, 50);
        gtk_widget_set_size_request (w2, -1, 50);
      } else {
        w = hpane = gtk_hpaned_new ();
        gtk_widget_set_size_request (w1, 50, -1);
        gtk_widget_set_size_request (w2, 50, -1);
      }
      gtk_paned_pack1 (GTK_PANED(w), w1, false, true);
      gtk_paned_pack2 (GTK_PANED(w), w2, true, false);
      gtk_paned_set_position (GTK_PANED(w), pos);
    }
    else if (w1!=NULL)
      w = w1;
    else if (w2!=NULL)
      w = w2;
    else
      w = NULL;
    return w;
  }
}

void GUI :: do_tabbed_layout (bool tabbed)
{
  if (hpane) {
    _prefs.set_int ("main-window-hpane-position", gtk_paned_get_position(GTK_PANED(hpane)));
    hpane = 0;
  }
  if (vpane) {
    _prefs.set_int ("main-window-vpane-position", gtk_paned_get_position(GTK_PANED(vpane)));
    vpane = 0;
  }

  gtk_widget_hide_all (_workarea_bin);

  GtkWidget * group_w (_group_pane->root());
  GtkWidget * header_w (_header_pane->root());
  GtkWidget * body_w (_body_pane->root());

  remove_from_parent (group_w);
  remove_from_parent (header_w);
  remove_from_parent (body_w);

  // remove workarea's current child
  GList * children = gtk_container_children (GTK_CONTAINER(_workarea_bin));
  if (children) {
    gtk_container_remove (GTK_CONTAINER(_workarea_bin), GTK_WIDGET(children->data));
    g_list_free (children);
  }

  GtkWidget * w (0);
  if (tabbed)
  {
    w = gtk_notebook_new ();
    GtkNotebook * n (GTK_NOTEBOOK (w));
    gtk_notebook_append_page (n, group_w, gtk_label_new_with_mnemonic (_("_1. Group Pane")));
    gtk_notebook_append_page (n, header_w, gtk_label_new_with_mnemonic (_("_2. Header Pane")));
    gtk_notebook_append_page (n, body_w, gtk_label_new_with_mnemonic (_("_3. Body Pane")));
    gtk_notebook_set_tab_border (n, PAD_SMALL);
  }
  else
  {
    GtkWidget *p[3];
    const std::string layout (_prefs.get_string ("pane-layout", "stacked-right"));
    const std::string orient (_prefs.get_string ("pane-orient", "groups,headers,body"));

    StringView tok, v(orient);
    v.pop_token(tok,','); if (*tok.str=='g') p[0]=group_w; else if (*tok.str=='h') p[0]=header_w; else p[0]=body_w;
    v.pop_token(tok,','); if (*tok.str=='g') p[1]=group_w; else if (*tok.str=='h') p[1]=header_w; else p[1]=body_w;
    v.pop_token(tok,','); if (*tok.str=='g') p[2]=group_w; else if (*tok.str=='h') p[2]=header_w; else p[2]=body_w;

    if (layout == "stacked-top") {
      w = pack_widgets (_prefs, p[0], p[1], HORIZONTAL, 1);
      w = pack_widgets (_prefs, w, p[2], VERTICAL, 0);
    } else if (layout == "stacked-bottom") {
      w = pack_widgets (_prefs, p[1], p[2], HORIZONTAL, 1);
      w = pack_widgets (_prefs, p[0], w, VERTICAL, 0);
    } else if (layout == "stacked-left") {
      w = pack_widgets (_prefs, p[0], p[1], VERTICAL, 0);
      w = pack_widgets (_prefs, w, p[2], HORIZONTAL, 1);
    } else if (layout == "stacked-vertical") {
      w = pack_widgets (_prefs, p[0], p[1], VERTICAL, 0);
      w = pack_widgets (_prefs, w, p[2], VERTICAL, 1);
    } else { // stacked right
      w = pack_widgets (_prefs, p[1], p[2], VERTICAL, 0);
      w = pack_widgets (_prefs, p[0], w,    HORIZONTAL, 1);
    }
  }

  gtk_container_add (GTK_CONTAINER(_workarea_bin), w);
  gtk_widget_show_all (_workarea_bin);
  set_visible (group_w, is_action_active("show-group-pane"));
  set_visible (header_w, is_action_active("show-header-pane"));
  set_visible (body_w, is_action_active("show-body-pane"));

  if (tabbed)
    gtk_notebook_set_current_page (GTK_NOTEBOOK(GTK_BIN(_workarea_bin)->child), 0);
}

void GUI :: do_show_group_pane (bool b)
{
  set_visible (_group_pane->root(), b);
}
void GUI :: do_show_header_pane (bool b)
{
  set_visible (_header_pane->root(), b);
}
void GUI :: do_show_body_pane (bool b)
{
  set_visible (_body_pane->root(), b);
}
void GUI :: do_show_toolbar (bool b)
{
  set_visible (_toolbar, b);
}
void GUI :: do_shorten_group_names (bool b)
{
  _group_pane->set_name_collapse (b);
}

void GUI :: do_match_only_unread_articles (bool b) { _header_pane->refilter (); }
void GUI :: do_match_only_cached_articles (bool b) { _header_pane->refilter (); }
void GUI :: do_match_only_binary_articles (bool b) { _header_pane->refilter (); }
void GUI :: do_match_only_my_articles (bool b) { _header_pane->refilter (); }
void GUI :: do_match_on_score_state (int state) { _header_pane->refilter (); }

void GUI :: do_show_matches (const Data::ShowType show_type)
{
  _header_pane->set_show_type (show_type);
}
void GUI :: do_quit ()
{
  gtk_main_quit ();
}
void GUI :: do_read_selected_group ()
{
  const Quark group (_group_pane->get_selection ());

  // update the titlebar
  if (group.empty())
    gtk_window_set_title (get_window(_root), _("Pan"));
  else {
    char * buf = g_strdup_printf (_("Pan: %s"), group.c_str());
    gtk_window_set_title (get_window(_root), buf);
    g_free (buf);
  }

  // set the charset encoding based upon that group's default
  if (!group.empty()) {
    std::string charset (_group_prefs.get_string (group, "character-encoding", "UTF-8"));
    charset.insert (0, "charset-");
    toggle_action (charset.c_str(), true);
  }

  // update the header pane
  watch_cursor_on ();
  const bool changed (_header_pane->set_group (group));
  _header_pane->set_focus ();
  watch_cursor_off ();

  // periodically save our state
  _data.save_state ();
  _prefs.save ();
  _group_prefs.save ();

  // maybe update the tab
  if (is_action_active ("tabbed-layout"))
    activate_action ("jump-to-header-tab");

  // update the body pane
  activate_action ("clear-body-pane");

  // if it's the first time in this group, pop up a download-headers dialog.
  // otherwise if get-new-headers is turned on, queue an xover-new task.
  if (changed && !group.empty() && _queue.is_online()) {
    unsigned long unread(0), total(0);
    _data.get_group_counts (group, unread, total);
    if (!total)
      activate_action ("download-headers");
    else if (_prefs.get_flag("get-new-headers-when-entering-group", true))
      _queue.add_task (new TaskXOver (_data, group, TaskXOver::NEW));
  }
}
void GUI :: do_mark_selected_groups_read ()
{
  const quarks_t group_names (_group_pane->get_full_selection ());
  foreach_const (quarks_t, group_names, it)
    _data.mark_group_read (*it);
}
void GUI :: do_clear_selected_groups ()
{
  Quark group (_group_pane->get_selection());
  _data.group_clear_articles (group);
}

void GUI :: do_xover_selected_groups ()
{
  const quarks_t group_names (_group_pane->get_full_selection ());
  foreach_const (quarks_t, group_names, it)
    _queue.add_task (new TaskXOver (_data, *it, TaskXOver::NEW));
}

void GUI :: do_xover_subscribed_groups ()
{
  typedef std::vector<Quark> quarks_v;
  quarks_v groups;
  _data.get_subscribed_groups (groups);
  foreach_const_r (quarks_v, groups, it)
    _queue.add_task (new TaskXOver (_data, *it, TaskXOver::NEW));
}

void GUI :: do_download_headers ()
{
  const quarks_t groups (_group_pane->get_full_selection ());
  headers_dialog (_data, _prefs, _queue, groups, get_window(_root));
}

void GUI :: do_refresh_groups ()
{
  std::vector<Task*> tasks;

  const quarks_t servers (_data.get_servers ());
  foreach_const_r (quarks_t, servers, it)
    tasks.push_back (new TaskGroups (_data, *it));

  if (!tasks.empty())
    _queue.add_tasks (tasks);
}

void GUI :: do_subscribe_selected_groups ()
{
  const quarks_t group_names (_group_pane->get_full_selection ());
  foreach (quarks_t, group_names, it)
    _data.set_group_subscribed (*it, true);
}
void GUI :: do_unsubscribe_selected_groups ()
{
  const quarks_t group_names (_group_pane->get_full_selection ());
  foreach (quarks_t, group_names, it)
    _data.set_group_subscribed (*it, false);
}
void GUI :: do_set_charset (const char * charset)
{
  _fallback_charset = charset;
  _body_pane->set_character_encoding (charset);
}

/***
****
***/

void
GUI :: refresh_connection_label ()
{
  char str[128];
  char tip[4096];

  const double KiBps = _queue.get_speed_KiBps ();
  int active, idle, connecting;
  _queue.get_connection_counts (active, idle, connecting);

  // decide what to say
  if (!_queue.is_online())
  {
    g_snprintf (str, sizeof(str), _("Offline"));

    if (active || idle)
      g_snprintf (tip, sizeof(tip), _("Closing %d connections"), (active+idle));
    else
      g_snprintf (tip, sizeof(tip), _("No Connections"));
  }
  else if (!active && !idle && connecting)
  {
    g_snprintf (str, sizeof(str), _("Connecting"));
    g_snprintf (tip, sizeof(tip), "%s", str);
  }
  else if (active || idle)
  { 
    typedef std::vector<Queue::ServerConnectionCounts> counts_t;
    counts_t counts;
    _queue.get_full_connection_counts (counts);
    int port;
    std::string s, addr;
    foreach_const (counts_t, counts, it) {
      _data.get_server_addr (it->server_id, addr, port);
      char buf[1024];
      g_snprintf (buf, sizeof(buf), _("%s: %d idle, %d active @ %.1f KiBps"),
                  addr.c_str(), it->idle, it->active, it->KiBps);
      s += buf;
      s += '\n';
    }
    if (!s.empty())
      s.resize (s.size()-1); // get rid of trailing linefeed

    g_snprintf (str, sizeof(str), "%d @ %.1f KiB/s", active, KiBps);
    g_snprintf (tip, sizeof(tip), "%s", s.c_str());
  }
  else
  {
    g_snprintf (str, sizeof(str), _("No Connections"));
    g_snprintf (tip, sizeof(tip), "%s", str);
  }

  gtk_label_set_text (GTK_LABEL(_connection_size_label), str);
  gtk_tooltips_set_tip (GTK_TOOLTIPS(_ttips), _connection_size_eventbox, tip, NULL);
}

namespace
{
  void
  timeval_diff (GTimeVal * start, GTimeVal * end, GTimeVal * diff)
  {
    diff->tv_sec = end->tv_sec - start->tv_sec;
    if (end->tv_usec < start->tv_usec)
      diff->tv_usec = 1000000ul + end->tv_usec - start->tv_usec, --diff->tv_sec;
    else
      diff->tv_usec = end->tv_usec - start->tv_usec;
  }
}

int
GUI :: upkeep_timer_cb (gpointer gui_g)
{
  static_cast<GUI*>(gui_g)->upkeep ();
  return true;
}

void
GUI :: upkeep ()
{
  refresh_connection_label ();
}

void
GUI :: set_queue_size_label (unsigned int running,
                             unsigned int size)
{
  char str[128];
  char tip[128];

  // build the format strings
  g_snprintf (tip, sizeof(tip), _("%u Tasks Running, %u Tasks Total"), running, size);
  if (!size)
    g_snprintf (str, sizeof(str), _("No Tasks"));
  else
    g_snprintf (str, sizeof(str), _("Tasks: %u/%u"), running, size);

  // update the gui
  gtk_label_set_text (GTK_LABEL(_queue_size_label), str);
  gtk_tooltips_set_tip (GTK_TOOLTIPS(_ttips), _queue_size_button, tip, NULL);
}

void
GUI :: on_queue_task_active_changed (Queue& q, Task& t, bool is_active)
{
  // update our set of active tasks
  std::list<Task*>::iterator it (std::find (_active_tasks.begin(), _active_tasks.end(), &t));
  if (is_active && it==_active_tasks.end())
    _active_tasks.push_back (&t);
  else if (!is_active && it!=_active_tasks.end())
    _active_tasks.erase (it);

  // update all the views
  int i (0);
  it = _active_tasks.begin();
  for (; it!=_active_tasks.end() && i<VIEW_QTY; ++it, ++i)
    _views[i]->set_progress (*it);
  for (; i<VIEW_QTY; ++i)
    _views[i]->set_progress (0);
}
void
GUI :: on_queue_connection_count_changed (Queue& q, int count)
{
  //connection_size = count;
  refresh_connection_label ();
}
void
GUI :: on_queue_size_changed (Queue& q, int active, int total)
{
  set_queue_size_label (active, total);
}
void
GUI :: on_queue_online_changed (Queue& q, bool is_online)
{
  toggle_action ("work-online", is_online);
}
void
GUI :: on_queue_error (Queue& q, const std::string& message)
{
  toggle_action ("work-online", false);

  std::string s (message);
  s += "\n \n";
  s += _("Pan is now offline. Please see \"File|Event Log\" and correct the problem, then use \"File|Work Online\" to continue.");
  Log::add_urgent_va ("%s", s.c_str());
}


void
GUI :: on_prefs_flag_changed (const StringView& key, bool value)
{
}
void
GUI :: on_prefs_string_changed (const StringView& key, const StringView& value)
{
  if (key == "pane-layout" || key == "pane-orient")
    GUI :: do_tabbed_layout (_prefs.get_flag ("tabbed-layout", false));

  if (key == "default-save-attachments-path")
    prev_path.assign (value.str, value.len);
}