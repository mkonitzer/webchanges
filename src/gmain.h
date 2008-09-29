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
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/splitter.h>
#include <wx/arrstr.h>

/*
 * WcTreeItemData - associate tree item with its underlying monitor
 */
class WcTreeItemData : public wxTreeItemData
{
public:
    WcTreeItemData(wxString name, wxString mf, wxString lastchk, wxString url);
    wxString GetName() { return name; }
    wxString GetMonFile() { return monfile; }
    wxString GetLastCheck() { return lastchk; }
    wxString GetURL() { return url; }
    void SetResult(bool oldres, bool curres);
    void SetResult(double oldres, double curres);
    void SetResult(xmlChar *oldres, xmlChar *curres);
    void SetResult(xmlXPathObjectPtr oldres, xmlXPathObjectPtr curres);

private:
    wxString name;
    wxString monfile;
    wxString lastchk;
    wxString url;

    wxArrayString oldres;
    wxArrayString curres;
};

/*
 * WcFrame - main window
 */
class WcFrame : public wxFrame
{
public:
    WcFrame(const wxChar *title);
    virtual ~WcFrame();
    void doCheck();

protected:
    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);

    wxTreeCtrl *treeCtrl;
    wxTextCtrl *resCtrl;
    wxListCtrl *logCtrl;

private:
    DECLARE_NO_COPY_CLASS(WcFrame)
    DECLARE_EVENT_TABLE()
};

/*
 * WcApp - gwebchanges application
 */
class WcApp : public wxApp
{
public:
    WcApp() { }
    virtual bool OnInit();

private:
    DECLARE_NO_COPY_CLASS(WcApp)
};

#endif /* __WC_GMAIN_H__ */
