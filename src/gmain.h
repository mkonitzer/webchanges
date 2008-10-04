/* $Id$ */
/* gwebchanges -- graphical user interface for webchanges

   Copyright (C) 2007  Marius Konitzer
   This file is part of webchanges.

   webchanges is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   webchanges is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with webchanges; if not, write to the Free Software Foundation,
   Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA  */

#ifndef __WC_GMAIN_H__
#define __WC_GMAIN_H__

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>
#include <wx/grid.h>
#include <libxml/xpath.h>
#include <libxml/list.h>
#include "monfile.h"
#include "basedir.h"

/*
 * WcTreeItemData - associate tree item with its underlying monitor
 */
class WcTreeItemData : public wxTreeItemData
  {
  public:
    WcTreeItemData (const monitorptr m, const metafileptr mef);
    const wxString& GetName () const
      {
        return name;
      }
    const wxString& GetLastCheck () const
      {
        return lastchk;
      }
    const wxArrayString& GetOldArray () const
      {
        return old;
      }
    const wxArrayString& GetCurArray () const
      {
        return cur;
      }
  private:
    wxString name;
    wxString lastchk;
    wxArrayString old;
    wxArrayString cur;
  };

class WcTreeCtrl : public wxTreeCtrl
  {
  public:
    WcTreeCtrl (wxWindow* parent, wxWindowID id);
    wxTreeItemId AppendMonfile (const monfileptr mf);
    wxTreeItemId AppendMonitor (const wxTreeItemId& parent, const monitorptr m, const metafileptr mef);
  private:
    void OnSelChanged(wxTreeEvent &event);
    DECLARE_EVENT_TABLE ()
  };

class WcResultGrid : public wxGrid
  {
  public:
    WcResultGrid (wxWindow* parent, wxWindowID id);
  private:
    void OnSize(wxSizeEvent& event);
    DECLARE_EVENT_TABLE ()
  };

class WcLogCtrl : public wxListCtrl
  {
  public:
    WcLogCtrl (wxWindow* parent, wxWindowID id);
  private:
    void OnSize(wxSizeEvent& event);
    DECLARE_EVENT_TABLE ()
  };

/*
 * WcFrame - main window
 */
class WcFrame : public wxFrame
  {
  public:
    WcFrame (const wxChar *title);
    int doInit (monfileptr mf);
    int doCheck (monfileptr mf, int update);
    int doRemove (monfileptr mf);
    void finalize (int count);

  protected:
    void OnQuit (wxCommandEvent& event);
    void OnAbout (wxCommandEvent& event);

  private:
    DECLARE_NO_COPY_CLASS (WcFrame)
    DECLARE_EVENT_TABLE ()
  };

/*
 * WcApp - gwebchanges application
 */
class WcApp : public wxApp
  {
  public:
    WcApp ();
    ~WcApp ();
    virtual void OnInitCmdLine (wxCmdLineParser &parser);
    virtual bool OnCmdLineError (wxCmdLineParser& parser);
    virtual bool OnCmdLineHelp (wxCmdLineParser& parser);
    virtual bool OnCmdLineParsed (wxCmdLineParser& parser);
    virtual bool OnInit ();

  private:
    DECLARE_NO_COPY_CLASS (WcApp)
    enum action
    {
      NONE, CHECK, INIT, UPDATE, REMOVE, TOOMANY
    };
    int action;
    char * userdir;
    basedirptr basedir;
    xmlListPtr filelist;

    const wxChar* usage (void);
    const wxChar* version (void);
    bool errexit (const wxChar *fmt, ...);
  };

#endif /* __WC_GMAIN_H__ */
