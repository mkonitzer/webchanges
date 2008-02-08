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

#include <libxml/tree.h>
#include <wx/wx.h>
#include <wx/wxchar.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>
#include <wx/string.h>
#include <time.h>
#include <errno.h>
#include "monfile.h"
#include "metafile.h"
#include "monitor.h"
#include "global.h"
#include "gmain.h"
#include "gmain16.xpm"
#include "gmain32.xpm"
#include "gmain48.xpm"
#include "config.h"

enum {
    LIST_ABOUT = wxID_ABOUT,
    LIST_QUIT = wxID_EXIT
};

wxListCtrl *logCtrl = NULL;

BEGIN_EVENT_TABLE(WcFrame, wxFrame)
    EVT_MENU(LIST_QUIT, WcFrame::OnQuit)
    EVT_MENU(LIST_ABOUT, WcFrame::OnAbout)
END_EVENT_TABLE()

IMPLEMENT_APP(WcApp)


int lvl_verbos = NOTICE;	/* level for stdout-verbosity */
int lvl_indent = 0;		/* level for indentation */
int force = 0;			/* force checking */

enum action
{ NONE, CHECK, INIT, UPDATE, REMOVE, TOOMANY };

/*
 * callback output-function
 */
void outputf(int l, const char *fmt, ...)
{
  va_list args;
  char str[1024];	// FIXME: allocating constant memory portion, ugly!

  if (lvl_verbos < l)
    return;

  va_start (args, fmt);
  vsnprintf((char*)&str, 1023, fmt, args);
  wxListItem *item = new wxListItem();
  item->SetId(logCtrl->GetItemCount());
  item->SetText(wxString(str, wxConvUTF8).c_str());
  switch (l) {
    case WARN:
	item->SetBackgroundColour(wxColour(255,255,120));
	break;
    case ERROR:
	item->SetBackgroundColour(wxColour(255,120,120));
  }
  logCtrl->InsertItem(*item);
  va_end (args);
}

/*
 * callback in-/outdentation-functions
 */
void indent (int l)
{
  if (lvl_verbos >= l && lvl_indent < 8)
    lvl_indent++;
}

void outdent (int l)
{
  if (lvl_verbos >= l && lvl_indent > 0)
    lvl_indent--;
}

/*
 * handle libxml errors
 */
static void
xml_errfunc (void *ctx, const char *msg, ...)
{
#ifdef SHOW_HTML_ERRORS
  va_list args;
  char str[1024];	// FIXME: allocating constant memory portion, ugly!
  if (verbosity < DEBUG)
    return;
  va_start (args, msg);
  vsnprintf((char*)&str, 1023, msg, args);
  wxListItem *item = new wxListItem();
  item->SetId(logCtrl->GetItemCount());
  item->SetText(wxString(str, wxConvUTF8).c_str());
  item->SetBackgroundColour(wxColour(255,255,120));
  logCtrl->InsertItem(*item);
  va_end (args);
#endif /* SHOW_HTML_ERRORS */
}

/*
 * print results, comparing @oldres to @curres
 */
void print_results (WcTreeItemData *wcid, xmlXPathObjectPtr oldres, xmlXPathObjectPtr curres)
{
  int i, j;
  /* results comparable? */
  if (oldres->type != curres->type)
    return;
  switch (oldres->type)
    {
    case XPATH_NODESET:
	wcid->SetResult(oldres->nodesetval, curres->nodesetval);
	break;
    case XPATH_STRING:
	wcid->SetResult(oldres->stringval, curres->stringval);
	break;
    case XPATH_NUMBER:
	wcid->SetResult(oldres->floatval, curres->floatval);
	break;
    case XPATH_BOOLEAN:
	wcid->SetResult(oldres->boolval?true:false, curres->boolval?true:false);
    default:
      break;
    }
}

/*
 * WcTreeItemData - associate tree item with its underlying monitor
 */
WcTreeItemData::WcTreeItemData(wxString name, wxString mf, wxString lastchk, wxString url)
{
    WcTreeItemData::name = name;
    WcTreeItemData::monfile = mf;
    WcTreeItemData::lastchk = lastchk;
    WcTreeItemData::url= url;
}

void
WcTreeItemData::SetResult(bool oldres, bool curres)
{
}

void
WcTreeItemData::SetResult(double oldres, double curres)
{
}

void
WcTreeItemData::SetResult(xmlChar *oldres, xmlChar *curres)
{
}

void
WcTreeItemData::SetResult(xmlXPathObjectPtr oldres, xmlXPathObjectPtr curres)
{
}

/*
 * WcFrame - main window
 */
WcFrame::WcFrame(const wxChar *title) : wxFrame(NULL, wxID_ANY, title)
{
    if (wxSystemSettings::GetScreenType() > wxSYS_SCREEN_SMALL)
        SetSize(wxSize(550, 400));

    // main window icon
    wxIconBundle iconsMain(wxICON(gmain32));
    iconsMain.AddIcon(wxICON(gmain16));
    iconsMain.AddIcon(wxICON(gmain48));
    SetIcons(iconsMain);
    // create menu bar
    wxMenu *menuFile = new wxMenu;
    menuFile->Append(LIST_QUIT, _T("E&xit\tAlt-X"));
    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(LIST_ABOUT, _T("&About"));
    wxMenuBar *menubar = new wxMenuBar;
    menubar->Append(menuFile, _T("&File"));
    menubar->Append(menuHelp, _T("&Help"));
    SetMenuBar(menubar);

    // create splitter
    wxSplitterWindow *spltWinH, *spltWinV;
    spltWinH = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxSize(200, 100));
    spltWinV = new wxSplitterWindow(spltWinH, wxID_ANY, wxDefaultPosition, wxSize(200, 100));
    
    // create tree control
    treeCtrl = new wxTreeCtrl(spltWinV, wxID_ANY, wxPoint(-1, -1), wxSize(200, 100), wxTR_DEFAULT_STYLE);
    wxTreeItemId id, root;
    root = treeCtrl->AddRoot(_T("Root"));
    id = treeCtrl->AppendItem(root, _T("123"));
    id = treeCtrl->AppendItem(root, _T("234"));

    // create result control
    resCtrl = new wxTextCtrl(spltWinV, wxID_ANY, wxEmptyString,
	    wxPoint(-1, -1), wxSize(200, 100), wxTE_MULTILINE |
	    wxSUNKEN_BORDER | wxTE_READONLY | wxTE_AUTO_URL); 

    // create log control
    logCtrl = ::logCtrl = new wxListCtrl(spltWinH, wxID_ANY, wxPoint(-1, -1), wxSize(200, 100), wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL | wxSUNKEN_BORDER);
    wxFont *ttFont = new wxFont(wxNORMAL_FONT->GetPointSize(),
	    wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    logCtrl->SetFont(*ttFont);
    // * create column
    wxListItem itemCol;
    itemCol.SetText(_T(""));
    logCtrl->InsertColumn(0, itemCol);
    // * adjust column widths
    logCtrl->SetColumnWidth(0, 1000);	    // FIXME: wxLIST_AUTOSIZE

    // finalize splitters
    spltWinV->SplitVertically(treeCtrl, resCtrl, 0);
    spltWinV->SetSashGravity(0.5);
    spltWinH->SplitHorizontally(spltWinV, logCtrl, 0);
    spltWinH->SetSashGravity(0.8);

    // create status bar
    CreateStatusBar(1);

    // setup relative positioning
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(spltWinH, 1, wxEXPAND);
    SetSizer(sizer);
    SetAutoLayout(true);
}

WcFrame::~WcFrame()
{
}

void WcFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
    Close(true);
}

void WcFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    wxMessageBox(wxString::Format(_("gwebchanges version %s\n"
	    "Graphical user interface for webchanges\n\n"
	    "Copyright (C) 2007 Marius Konitzer\n\n"
	    "This is free software; see the source for copying conditions. "
	    "There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR "
	    "A PARTICULAR PURPOSE, to the extent permitted by law."),
	    wxT(VERSION)),
	_("About gwebchanges"), wxOK | wxICON_INFORMATION);
}

void WcFrame::doCheck()
{
    int update=0;
    monitorptr m;
    metafileptr mef;
    xmlChar *mfname;
    int ret, count = 0;
    vpairptr lastvp = NULL;
    monfileptr mf;
    /* open monitor file @mf */
    mf = monfile_open ("test.xml");
    mfname = monfile_get_name (mf);
    /* read monitor file @mf */
    outputf (NOTICE, "Monitor File %s\n", mfname);
    indent (NOTICE);
    /* read metadata file @mef */
    mef = metafile_open (mf);
    metafile_read (mef);
    while ((ret = monfile_get_next_monitor (mf, &m)) != RET_ERROR)
    {
      time_t nextchk;
      xmlChar *name;
      if (ret == RET_EOF)
	break;
      /* we obtained a monitor @m */
      name = monitor_get_name (m);
      nextchk = monitor_get_next_check (mef, m);
      if (force != 0 || time (NULL) >= nextchk)
	{
	  /* checking of monitor @m is necessary */
	  outputf (NOTICE, "Checking %s now:\n", name);
	  indent (NOTICE);
	  if ((ret = monitor_evaluate (m)) == RET_OK)
	    {
	      /* monitor @m was evaluable */
	      if (monitor_triggered (m) != 0)
		{
		  /* monitor @m reported a change */
		  outputf (WARN, "%s (%s):\n", name, mfname);
		  indent (WARN);
		  //print_results (WARN, monitor_get_old_result (m),
			//	 monitor_get_cur_result (m));
		  outputf (WARN, "\n");
		  outdent (WARN);
		  /* update cache file, if requested */
		  if (update != 0)
		    {
		      vpairptr vp = monitor_get_vpair (m);
		      if (lastvp != vp)
			{
			  /* update of @vp necessary */
			  outputf (NOTICE, "Updating %s\n",
				   vpair_get_cache (vp));
			  indent (NOTICE);
			  vpair_download (vp);
			  outdent (NOTICE);
			  lastvp = vp;
			}
		      else
			outputf (DEBUG, "Skipping update, already done\n");
		    }
		  count++;
		}
	      else
		outputf (NOTICE, "%s NOT triggered.\n", name);
	      monitor_set_last_check (mef, m, time (NULL));
	    }
	  else
	    outputf (WARN, "Skipping %s (%s), not evaluable.\n", name,
		     mfname);
	  outdent (NOTICE);
	}
      else
	outputf (NOTICE, "Skipping %s, next checking %s", name,
		 ctime (&nextchk));
      monitor_free (m);
    }
    /* close metadata file @mef */
    if (ret != RET_ERROR)
    metafile_write (mef);
    metafile_close (mef);
    outdent (NOTICE);
}


/*
 * WcApp - gwebchanges application
 */
bool WcApp::OnInit()
{
    WcFrame *frame = new WcFrame(wxT("gwebchanges"));

    // register error function
    xmlSetGenericErrorFunc (NULL, xml_errfunc);    

    // perform the specified action
    frame->doCheck();

    frame->Show(true);
    SetTopWindow(frame);

    return true;
}
