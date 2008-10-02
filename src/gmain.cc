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

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>
#include <wx/cmdline.h>
#include <wx/grid.h>
#include <libxml/xpath.h>
#include <libxml/list.h>
#include "monfile.h"
#include "metafile.h"
#include "monitor.h"
#include "global.h"
#include "basedir.h"
#include "gmain.h"
#include "gmain16.xpm"
#include "gmain32.xpm"
#include "gmain48.xpm"
#include "config.h"

/*
 * Translate wxString into (char*) for use in system calls.
 */
#if defined(__WXMAC__)
#define OSFILENAME(X) ((char *) (const char *)(X).fn_str())
#else
#define OSFILENAME(X) ((char *) (const char *)(X).mb_str())
#endif

enum
{
  ID_LIST_ABOUT = wxID_ABOUT,
  ID_LIST_QUIT = wxID_EXIT,
  ID_TREECTRL
};

WcLogCtrl *logCtrl = NULL;
WcResultGrid *resGrid = NULL;
WcTreeCtrl *treeCtrl = NULL;

BEGIN_EVENT_TABLE(WcTreeCtrl, wxTreeCtrl)
EVT_TREE_SEL_CHANGED(ID_TREECTRL, WcTreeCtrl::OnSelChanged)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE (WcResultGrid, wxGrid)
EVT_SIZE (WcResultGrid::OnSize)
END_EVENT_TABLE ()

BEGIN_EVENT_TABLE (WcLogCtrl, wxListCtrl)
EVT_SIZE (WcLogCtrl::OnSize)
END_EVENT_TABLE ()

BEGIN_EVENT_TABLE (WcFrame, wxFrame)
EVT_MENU (ID_LIST_QUIT, WcFrame::OnQuit)
EVT_MENU (ID_LIST_ABOUT, WcFrame::OnAbout)
END_EVENT_TABLE ()

IMPLEMENT_APP (WcApp)


int lvl_verbos = LVL_NOTICE; /* level for stdout-verbosity */
int lvl_indent = 0; /* level for indentation */
bool force = false; /* force checking */

/*
 * callback output-function
 */
void
outputf (int l, const char *fmt, ...)
{
  va_list args;
  char str[1024]; // FIXME: allocating constant memory portion, ugly!

  if (lvl_verbos < l)
    return;

  va_start (args, fmt);
  vsnprintf ((char*) & str, 1023, fmt, args);
  wxListItem item;
  item.SetId (logCtrl->GetItemCount ());
  item.SetText (wxString (str, wxConvUTF8).Trim());
  switch (l)
    {
    case LVL_WARN:
      item.SetBackgroundColour (wxColour (255, 255, 120));
      break;
    case LVL_ERR:
      item.SetBackgroundColour (wxColour (255, 120, 120));
    }
  logCtrl->InsertItem (item);
  va_end (args);
}

/*
 * callback in-/outdentation-functions
 */
void
indent (int l)
{
  if (lvl_verbos >= l && lvl_indent < 8)
    lvl_indent++;
}

void
outdent (int l)
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
  char str[1024]; // FIXME: allocating constant memory portion, ugly!
  if (verbosity < LVL_DEBUG)
    return;
  va_start (args, msg);
  vsnprintf ((char*) & str, 1023, msg, args);
  wxListItem *item = new wxListItem ();
  item->SetId (logCtrl->GetItemCount ());
  item->SetText (wxString (str, wxConvUTF8).c_str ());
  item->SetBackgroundColour (wxColour (255, 255, 120));
  logCtrl->InsertItem (*item);
  va_end (args);
#endif /* SHOW_HTML_ERRORS */
}

static void
xml_strlist_deallocator (xmlLinkPtr lk)
{
  if (lk == NULL)
    return;
  void *data = xmlLinkGetData (lk);
  if (data != NULL)
    free (data);
}

/*
 * WcTreeItemData - associate tree item with its underlying monitor
 */
WcTreeItemData::WcTreeItemData (const monitorptr m, const metafileptr mef)
{
  name = wxString ((char*) monitor_get_name(m), wxConvUTF8);
  lastchk = wxDateTime (monitor_get_last_check(mef, m)).Format ();

  /* results comparable? */
  xmlXPathObjectPtr oldres = monitor_get_old_result(m);
  xmlXPathObjectPtr curres = monitor_get_cur_result(m);
  if (oldres->type != curres->type)
    return;
  old.Clear ();
  cur.Clear ();
  switch (oldres->type)
    {
    case XPATH_NODESET:
      for (int i = 0; i < 2; ++i)
	{
	  const xmlNodeSetPtr nodes = (i == 0 ? oldres : curres)->nodesetval;
	  wxArrayString& strings = (i == 0 ? old : cur);
	  /* print nodes in node-set sequentially */
	  for (int j = 0; j < (nodes ? nodes->nodeNr : 0); ++j)
	    {
              wxString str, val;
	      xmlNodePtr cur = nodes->nodeTab[j];
	      switch (cur->type)
		{
		case XML_ATTRIBUTE_NODE:
                  str = wxString((char *) cur->name, wxConvUTF8);
                  val = wxString((char *) cur->children->content, wxConvUTF8);
                  strings.Add (_("(ATTR): ") + str + _(" = \"") + val + _("\""));
		  break;
		case XML_COMMENT_NODE:
                  str = wxString((char *) cur->content, wxConvUTF8);
                  strings.Add (_ ("(COMM): ") + str);
		  break;
		case XML_ELEMENT_NODE:
                  str = wxString((char *) cur->name, wxConvUTF8);
                  strings.Add (_ ("(ELEM): ") + str);
		  break;
		case XML_TEXT_NODE:
                  str = wxString((char *) cur->content, wxConvUTF8);
                  strings.Add (_ ("(TEXT): ") + str);
		default:
		  break;
		}
	    }
	}
      break;
    case XPATH_STRING:
      old.Add (wxString ((char *) oldres->stringval, wxConvUTF8));
      cur.Add (wxString ((char *) curres->stringval, wxConvUTF8));
      break;
    case XPATH_NUMBER:
      old.Add (wxString::Format(_ ("%.2lf"), oldres->floatval));
      cur.Add (wxString::Format(_ ("%.2lf"), curres->floatval));
      break;
    case XPATH_BOOLEAN:
      old.Add (oldres->boolval == 0 ? _ ("FALSE") : _ ("TRUE"));
      cur.Add (curres->boolval == 0 ? _ ("FALSE") : _ ("TRUE"));
    default:
      break;
    }
}

WcTreeCtrl::WcTreeCtrl (wxWindow* parent, wxWindowID id) : wxTreeCtrl (parent, id, wxPoint (-1, -1), wxSize (200, 100), wxTR_DEFAULT_STYLE)
{
  wxTreeItemId root = AddRoot (_T ("Monitor files"));
  SetItemData (root, NULL);
}

wxTreeItemId
WcTreeCtrl::AppendMonfile (const monfileptr mf)
{
    const xmlChar* name = monfile_get_name(mf);
    wxString wxname = wxString ((char *) name, wxConvUTF8);
    wxTreeItemId node = AppendItem (GetRootItem (), wxname);
    SetItemData (node, NULL);
    return node;
}

wxTreeItemId
WcTreeCtrl::AppendMonitor (const wxTreeItemId& parent, const monitorptr m, const metafileptr mef)
{
    const xmlChar* name = monitor_get_name(m);
    wxTreeItemId node = AppendItem (parent, wxString ((char *) name, wxConvUTF8));
    SetItemData (node, new WcTreeItemData(m, mef));
    return node;
}

void
WcTreeCtrl::OnSelChanged(wxTreeEvent &event)
{
  const WcTreeItemData* data = (WcTreeItemData*) GetItemData (event.GetItem());
  if (data == NULL)
  {
    resGrid->BeginBatch();
    resGrid->SetColLabelValue (0, _ ("Old Result"));
    resGrid->DeleteRows (0, resGrid->GetNumberRows ());
    resGrid->EndBatch();
    return;
  }
  const wxArrayString& cur = data->GetCurArray();
  const wxArrayString& old = data->GetOldArray();
  unsigned int rows = old.GetCount();
  if (cur.GetCount() > rows)
      rows = cur.GetCount();

  resGrid->BeginBatch();
  resGrid->SetColLabelValue (0, wxString::Format(_ ("Old Result (%s)"), data->GetLastCheck ().c_str ()));
  resGrid->DeleteRows (0, resGrid->GetNumberRows ());
  resGrid->AppendRows (rows);
  for (unsigned int i = 0; i < old.GetCount(); ++i)
    resGrid->SetCellValue (i, 0, old[i]);
  for (unsigned int i = 0; i < cur.GetCount(); ++i)
    resGrid->SetCellValue (i, 1, cur[i]);
  resGrid->Show ();
  resGrid->EndBatch();
}


WcResultGrid::WcResultGrid (wxWindow* parent, wxWindowID id) : wxGrid(parent, id, wxPoint (-1, -1), wxSize (200, 100))
{
  CreateGrid (0, 2);
  SetColLabelValue (0, _ ("Old Result"));
  SetColLabelValue (1, _ ("Current Result"));
  EnableEditing (false);
  EnableDragGridSize (false);
  EnableDragRowSize (false);
}

void
WcResultGrid::OnSize (wxSizeEvent& event)
{
  wxGrid::OnSize (event);
  // Calculate width of single column
  const wxSize& frame_size = event.GetSize ();
  int column_width = (frame_size.GetWidth () - GetRowLabelSize ());
  column_width -= 2 * wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);
  column_width /= 2;
  if (column_width < GetColMinimalWidth(0))
    column_width = GetColMinimalWidth(0);
  // Resize both columns
  BeginBatch ();
  SetColSize(0, column_width);
  SetColSize(1, column_width);
  EndBatch ();
}

WcLogCtrl::WcLogCtrl (wxWindow* parent, wxWindowID id) : wxListCtrl (parent, id, wxPoint (-1, -1), wxSize (200, 100), wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL | wxSUNKEN_BORDER)
{
  // Use typewriter/teletype font
  wxFont ttFont (wxNORMAL_FONT->GetPointSize (), wxFONTFAMILY_TELETYPE,
          wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
  SetFont (ttFont);
  // Create single column
  wxListItem itemCol;
  itemCol.SetText (_T (""));
  InsertColumn (0, itemCol);
}

void
WcLogCtrl::OnSize (wxSizeEvent& event)
{
  wxListCtrl::OnSize (event);
  // Calculate width of column
  int column_width = event.GetSize ().GetWidth ();
  column_width -= 2 * wxSystemSettings::GetMetric (wxSYS_VSCROLL_X);
  // Resize column
  SetColumnWidth (0, column_width);
}

/*
 * WcFrame - main window
 */
WcFrame::WcFrame (const wxChar *title) : wxFrame (NULL, wxID_ANY, title)
{
  if (wxSystemSettings::GetScreenType () > wxSYS_SCREEN_SMALL)
    SetSize (wxSize (550, 400));

  // main window icon
  wxIconBundle iconsMain (wxICON (gmain32));
  iconsMain.AddIcon (wxICON (gmain16));
  iconsMain.AddIcon (wxICON (gmain48));
  SetIcons (iconsMain);
  // create menu bar
  wxMenu *menuFile = new wxMenu;
  menuFile->Append (ID_LIST_QUIT, _T ("E&xit\tAlt-X"));
  wxMenu *menuHelp = new wxMenu;
  menuHelp->Append (ID_LIST_ABOUT, _T ("&About"));
  wxMenuBar *menubar = new wxMenuBar;
  menubar->Append (menuFile, _T ("&File"));
  menubar->Append (menuHelp, _T ("&Help"));
  SetMenuBar (menubar);

  // create splitter
  wxSplitterWindow *spltWinH, *spltWinV;
  spltWinH = new wxSplitterWindow (this, wxID_ANY, wxDefaultPosition, wxSize (200, 100));
  spltWinV = new wxSplitterWindow (spltWinH, wxID_ANY, wxDefaultPosition, wxSize (200, 100));

  // create tree control
  treeCtrl = new WcTreeCtrl (spltWinV, ID_TREECTRL);

  // create result grid
  resGrid = new WcResultGrid (spltWinV, wxID_ANY);

  // create log control
  logCtrl = new WcLogCtrl (spltWinH, wxID_ANY);

  // finalize splitters
  spltWinV->SplitVertically (treeCtrl, resGrid, 0);
  spltWinV->SetSashGravity (0.1);
  spltWinH->SplitHorizontally (spltWinV, logCtrl, 0);
  spltWinH->SetSashGravity (0.75);

  // create status bar
  CreateStatusBar (1);

  // setup relative positioning
  wxBoxSizer *sizer = new wxBoxSizer (wxVERTICAL);
  sizer->Add (spltWinH, 1, wxEXPAND);
  SetSizer (sizer);
  SetAutoLayout (true);
}

void
WcFrame::OnQuit (wxCommandEvent& WXUNUSED (event))
{
  Close (true);
}

void
WcFrame::OnAbout (wxCommandEvent& WXUNUSED (event))
{
  wxMessageBox (wxString::Format (_ ("gwebchanges version %s\n"
                                     "Graphical user interface for webchanges\n\n"
                                     "Copyright (C) 2007 Marius Konitzer\n\n"
                                     "This is free software; see the source for copying conditions. "
                                     "There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR "
                                     "A PARTICULAR PURPOSE, to the extent permitted by law."),
                                  wxT (VERSION)),
                _ ("About gwebchanges"), wxOK | wxICON_INFORMATION);
}

/*
 * initialize: download all referenced documents
 */
int
WcFrame::doInit (monfileptr mf)
{
  int ret;
  vpairptr vp;
  /* read monitor file @mf */
  outputf (LVL_NOTICE, "Monitor File %s\n", monfile_get_name (mf));
  indent (LVL_NOTICE);
  while ((ret = monfile_get_next_vpair (mf, &vp)) != RET_EOF)
    {
      if (ret == RET_ERROR)
        break;
      outputf (LVL_NOTICE, "Downloading %s\n", vpair_get_url (vp));
      indent (LVL_NOTICE);
      outputf (LVL_NOTICE, "=> %s\n", vpair_get_cache (vp));
      vpair_download (vp);
      outdent (LVL_NOTICE);
      vpair_close (vp);
    }
  outdent (LVL_NOTICE);
  return (ret != RET_ERROR ? RET_OK : RET_ERROR);
}

/*
 * check: print, which monitors have changed and how (and @update cache)
 */
int
WcFrame::doCheck (monfileptr mf, int update)
{
  monitorptr m;
  metafileptr mef;
  const xmlChar *mfname;
  int ret, count = 0;
  vpairptr lastvp = NULL;
  wxTreeItemId mf_node;
  /* read monitor file @mf */
  mfname = monfile_get_name (mf);
  outputf (LVL_NOTICE, "Monitor File %s\n", mfname);
  indent (LVL_NOTICE);
  /* read metadata file @mef */
  mef = metafile_open (mf);
  metafile_read (mef);
  while ((ret = monfile_get_next_monitor (mf, &m)) != RET_ERROR)
    {
      time_t nextchk;
      const xmlChar *name;
      if (ret == RET_EOF)
        break;
      /* we obtained a monitor @m */
      name = monitor_get_name (m);
      nextchk = monitor_get_next_check (mef, m);
      if (force != 0 || time (NULL) >= nextchk)
        {
          /* checking of monitor @m is necessary */
          outputf (LVL_NOTICE, "Checking %s now:\n", name);
          indent (LVL_NOTICE);
          if ((ret = monitor_evaluate (m)) == RET_OK)
            {
              /* monitor @m was evaluable */
              if (monitor_triggered (m) != 0)
                {
                  /* monitor @m reported a change */
                  outputf (LVL_WARN, "%s (%s):\n", name, mfname);
                  indent (LVL_WARN);
                  if (!mf_node.IsOk ())
                      mf_node = treeCtrl->AppendMonfile (mf);
                  treeCtrl->AppendMonitor (mf_node, m, mef);
                  outputf (LVL_WARN, "see above.\n");
                  outdent (LVL_WARN);
                  /* update cache file, if requested */
                  if (update != 0)
                    {
                      vpairptr vp = monitor_get_vpair (m);
                      if (lastvp != vp)
                        {
                          /* update of @vp necessary */
                          outputf (LVL_NOTICE, "Updating %s\n",
                                   vpair_get_cache (vp));
                          indent (LVL_NOTICE);
                          vpair_download (vp);
                          outdent (LVL_NOTICE);
                          lastvp = vp;
                        }
                      else
                        outputf (LVL_DEBUG, "Skipping update, already done\n");
                    }
                  count++;
                }
              else
                outputf (LVL_NOTICE, "%s NOT triggered.\n", name);
              monitor_set_last_check (mef, m, time (NULL));
            }
          else
            outputf (LVL_WARN, "Skipping %s (%s), not evaluable.\n", name,
                     mfname);
          outdent (LVL_NOTICE);
        }
      else
        outputf (LVL_NOTICE, "Skipping %s, next checking %s", name,
                 ctime (&nextchk));
      monitor_free (m);
    }
  /* close metadata file @mef */
  if (ret != RET_ERROR)
    metafile_write (mef);
  metafile_close (mef);
  outdent (LVL_NOTICE);
  return (ret != RET_ERROR ? count : RET_ERROR);
}

/*
 * remove: remove referenced files from cache
 */
int
WcFrame::doRemove (monfileptr mf)
{
  int ret;
  vpairptr vp;
  /* read monitor file @mf */
  outputf (LVL_NOTICE, "Monitor File %s\n", monfile_get_name (mf));
  indent (LVL_NOTICE);
  while ((ret = monfile_get_next_vpair (mf, &vp)) != RET_EOF)
    {
      if (ret == RET_ERROR)
        break;
      outputf (LVL_NOTICE, "Removing %s from cache\n", vpair_get_url (vp));
      indent (LVL_NOTICE);
      vpair_remove (vp);
      outdent (LVL_NOTICE);
      vpair_close (vp);
    }
  outdent (LVL_NOTICE);
  return (ret != RET_ERROR ? RET_OK : RET_ERROR);
}

void
WcFrame::finalize (int count)
{
  // Adjust log column width
  logCtrl->SetColumnWidth (0, wxLIST_AUTOSIZE);
  if (count < 0)
    SetStatusText (_ ("Error(s) occured, see log for details."));
  else
    SetStatusText (wxString::Format (_ ("webchanges reported %d triggered monitor(s)."), count));
}

/*
 * WcApp - gwebchanges application
 */
WcApp::WcApp ()
{
  action = NONE;
  userdir = NULL;
  basedir = NULL;
  filelist = xmlListCreate (xml_strlist_deallocator, NULL);
}

WcApp::~WcApp ()
{
  if (filelist != NULL)
    {
      xmlListDelete (filelist);
      filelist = NULL;
    }
  if (basedir != NULL)
    {
      basedir_close (basedir);
      basedir = NULL;
    }
  if (userdir != NULL)
    {
      free (userdir);
      userdir = NULL;
    }
}
const wxChar*
WcApp::usage (void)
{
  return wxT ("Usage: webchanges COMMAND [OPTION]... FILE...\n\n" \
  "Commands:\n" \
  "  -i  initialize monitor file, download into cache\n" \
  "  -c  check monitor file for changes\n" \
  "  -u  check monitor file for changes and update cache\n" \
  "  -r  remove files associated with monitor file from cache\n" \
  "  -h  display this help and exit\n" \
  "  -V  display version & copyright information and exit\n\n" \
  "Options:\n" \
  "  -f  force checking/updating of all monitors now\n" \
  "  -b  set base directory\n" \
  "  -q  quiet mode, suppress most stdout messages\n" \
  "  -v  verbose mode, repeat to increase stdout messages");
}

const wxChar*
WcApp::version (void)
{
  return wxT ("webchanges version %s\n" VERSION \
  "Copyright (C) 2006, 2007 Marius Konitzer\n" \
  "This is free software; see the source for copying conditions.  " \
  "There is NO\nwarranty; not even for MERCHANTABILITY or FITNESS " \
  "FOR A PARTICULAR PURPOSE,\nto the extent permitted by law.");
}

bool
WcApp::errexit (const wxChar *fmt, ...)
{
  wxString errmsg;
  va_list args;
  va_start (args, fmt);
  errmsg = wxString::FormatV (fmt, args) + _ ("\n") + usage ();
  va_end (args);
  wxMessageBox (errmsg, _ ("Error"), wxICON_ERROR);
  return false;
}

void
WcApp::OnInitCmdLine (wxCmdLineParser &parser)
{
  wxApp::OnInitCmdLine (parser);
  static const wxCmdLineEntryDesc cmdLineDesc[] ={
    { wxCMD_LINE_SWITCH, _ ("i"), NULL, NULL},
    { wxCMD_LINE_SWITCH, _ ("c"), NULL, NULL},
    { wxCMD_LINE_SWITCH, _ ("u"), NULL, NULL},
    { wxCMD_LINE_SWITCH, _ ("r"), NULL, NULL},
    { wxCMD_LINE_SWITCH, _ ("h"), NULL, NULL, wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP},
    { wxCMD_LINE_SWITCH, _ ("V"), NULL, NULL},

    { wxCMD_LINE_SWITCH, _ ("f"), NULL, NULL},
    { wxCMD_LINE_OPTION, _ ("b"), NULL, NULL},
    { wxCMD_LINE_SWITCH, _ ("v"), NULL, NULL, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
    { wxCMD_LINE_SWITCH, _ ("q"), NULL, NULL},

    { wxCMD_LINE_PARAM, NULL, NULL, NULL, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_PARAM_MULTIPLE},

    { wxCMD_LINE_NONE}
  };
  parser.SetDesc (cmdLineDesc);
  parser.SetSwitchChars (_ ("-"));
  parser.DisableLongOptions ();
}

bool
WcApp::OnCmdLineError (wxCmdLineParser& parser)
{
  wxMessageBox (usage (), _ ("Usage"), wxICON_ERROR);
  return false;
}

bool
WcApp::OnCmdLineHelp (wxCmdLineParser& parser)
{
  wxMessageBox (usage (), _ ("Usage"), wxICON_INFORMATION);
  return false;
}

bool
WcApp::OnCmdLineParsed (wxCmdLineParser& parser)
{
  /* Actions */
  if (parser.Found (_ ("i")))
    action = (action == NONE ? INIT : TOOMANY);
  if (parser.Found (_ ("c")))
    action = (action == NONE ? CHECK : TOOMANY);
  if (parser.Found (_ ("u")))
    action = (action == NONE ? UPDATE : TOOMANY);
  if (parser.Found (_ ("r")))
    action = (action == NONE ? REMOVE : TOOMANY);
  if (parser.Found (_ ("V")))
    {
      wxMessageBox (version (), _ ("Version info"), wxICON_INFORMATION);
      return false;
    }
  /* Options */
  if (parser.Found (_ ("f")))
    force = true;
  wxString wxstr;
  if (parser.Found (_ ("b"), &wxstr))
    userdir = strdup (OSFILENAME (wxstr));
  if (parser.Found (_ ("v")))
    lvl_verbos++;
  if (parser.Found (_ ("q")))
    lvl_verbos = LVL_WARN;
  /* Parameters */
  for (unsigned int i = 0; i < parser.GetParamCount (); ++i)
    xmlListPushBack (filelist, strdup (OSFILENAME (parser.GetParam (i))));
  return true;
}

bool
WcApp::OnInit ()
{
  int count = 0;

  /* Display messages as message boxes. */
  wxMessageOutput::Set (new wxMessageOutputMessageBox);

  /* Prepare main window. */
  WcFrame *frame = new WcFrame (wxT ("gwebchanges"));

  /* Register error function. */
  xmlSetGenericErrorFunc (NULL, xml_errfunc);

  /* Parse command line. */
  if (!wxApp::OnInit ())
    return false;

  /* Do we have a unique command? */
  if (action == NONE)
    return errexit (_ ("No command given, exiting."));
  if (action == TOOMANY)
    return errexit (_ ("More than one command given, exiting."));

  /* Setup basedir. */
  basedir = basedir_open (userdir);
  if (basedir == NULL)
    return errexit (_ ("Could not determine base directory."));
  /* Monitor files must be passed on cmdline when using current directory. */
  if (basedir_is_curdir (basedir) != 0 && xmlListEmpty (filelist) != 0)
    {
      basedir_close (basedir);
      return errexit (_ ("No monitor file(s) given, exiting."));
    }
  /* Prepare base directory if necessary. */
  if (basedir_is_prepared (basedir) == 0)
    {
      if (basedir_prepare (basedir) != RET_OK)
        {
          basedir_close (basedir);
          return errexit (_ ("Could not prepare base directory, exiting."));
        }
    }

  /* Build file list by taking all monitor files in basedir. */
  if (xmlListEmpty (filelist) != 0)
    basedir_get_all_monfiles (basedir, filelist);
  if (xmlListEmpty (filelist) != 0)
    {
      basedir_close (basedir);
      return errexit (_ ("No monitor files found, exiting."));
    }

  /* Walk through all monitor files found. */
  while (xmlListEmpty (filelist) == 0)
    {
      int ret = 0;
      monfileptr mf;
      xmlLinkPtr lk;
      const char *filename;
      /* Get next monitor file from file list */
      lk = xmlListFront (filelist);
      filename = (const char*) xmlLinkGetData (lk);

      /* Open monitor file. */
      mf = monfile_open (filename, basedir);
      if (mf == NULL)
        {
          /* Skip this monitor file. */
          outputf (LVL_ERR, "Error reading monitor file %s\n\n", filename);
          xmlListPopFront (filelist);
          count = -1;
          continue;
        }
      /* Process monitor file. */
      outputf (LVL_INFO, "Processing monitor file %s\n", filename);
      switch (action)
        {
        case INIT:
          ret = frame->doInit (mf);
          break;
        case CHECK:
          ret = frame->doCheck (mf, 0);
          break;
        case UPDATE:
          ret = frame->doCheck (mf, 1);
          break;
        case REMOVE:
          ret = frame->doRemove (mf);
          break;
        }
      count = (ret < 0 || count < 0 ? -1 : count + ret);
      /* Ready to close monitor file. */
      monfile_close (mf);
      xmlListPopFront (filelist);
    }
  basedir_close (basedir);
  basedir = NULL;
  xmlListDelete (filelist);
  filelist = NULL;
  xmlCleanupParser ();

  /* Exit if nothing has happened. */
  if (logCtrl->GetItemCount () == 0 && treeCtrl->GetCount () == 1)
      return false;

  /* Display main window. */
  frame->finalize (count);
  frame->Show (true);
  SetTopWindow (frame);

  return true;
}
