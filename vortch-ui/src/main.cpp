#include <wx/wx.h>
#include <wx/bmpbndl.h>
#include <wx/dcbuffer.h>
#include <wx/taskbar.h>
#include <wx/dnd.h>
#include <wx/hyperlink.h>

#include <string>
#include <vector>

#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <optional>
#include <filesystem>

#include "config.hpp"
#include "platform.hpp"
#include "vortch/launch.hpp"
#include "vortch/store.hpp"
#include "vortch/text.hpp"

namespace {
wxString AssetPath(const char* name) {
  return wxString::FromUTF8(VORTCH_ASSETS_DIR) + "/" + name;
}
const wxColour kAccent(49, 196, 255);  // move-mode highlight
constexpr int kFullSize = 96;
constexpr int kPeekSize = 32;

bool hasFlag(const std::vector<std::string>& a, const char* f) {
  for (const auto& s : a) if (s == f) return true;
  return false;
}
std::string flagValue(const std::vector<std::string>& a, const char* f) {
  for (std::size_t i = 0; i + 1 < a.size(); ++i) if (a[i] == f) return a[i + 1];
  return {};
}
} // namespace

class VortchFrame;

// Tray icon: show/hide the widget + quit.
class VortchTray : public wxTaskBarIcon {
public:
  VortchTray(const wxBitmapBundle& bundle, wxFrame* frame) : frame_(frame) {
    wxIcon icon;
    icon.CopyFromBitmap(bundle.GetBitmap(wxSize(32, 32)));
    SetIcon(icon, "vortch");
    Bind(wxEVT_MENU, &VortchTray::OnMenu, this);
  }
  wxMenu* CreatePopupMenu() override {
    auto* menu = new wxMenu();
    menu->Append(ID_TOGGLE, frame_->IsShown() ? "Hide widget" : "Show widget");
    menu->AppendSeparator();
    menu->Append(wxID_EXIT, "Quit vortch");
    return menu;
  }
private:
  enum { ID_TOGGLE = wxID_HIGHEST + 100 };
  void OnMenu(wxCommandEvent& e) {
    if (e.GetId() == ID_TOGGLE)      frame_->Show(!frame_->IsShown());
    else if (e.GetId() == wxID_EXIT) wxTheApp->ExitMainLoop();
  }
  wxFrame* frame_;
};

enum class ZMode { Topmost, OnDesktop };

// Borderless, always-on-top desktop-icon-style window (Tier C): context menu,
// move mode, switchable z-order, and a "peek" mode that hover-expands.
class VortchFrame : public wxFrame {
public:
  explicit VortchFrame(wxBitmapBundle bundle)
      : wxFrame(nullptr, wxID_ANY, "vortch", wxDefaultPosition,
                wxSize(kFullSize, kFullSize),
                wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP | wxBORDER_NONE),
        bundle_(std::move(bundle)) {
    SetClientSize(kFullSize, kFullSize);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    escTimer_.SetOwner(this, ID_TIMER_ESC);
    animTimer_.SetOwner(this, ID_TIMER_ANIM);
    leaveTimer_.SetOwner(this, ID_TIMER_LEAVE);
    Bind(wxEVT_PAINT,               &VortchFrame::OnPaint,       this);
    Bind(wxEVT_CONTEXT_MENU,        &VortchFrame::OnContextMenu, this);
    Bind(wxEVT_MENU,                &VortchFrame::OnMenu,        this);
    Bind(wxEVT_LEFT_DOWN,           &VortchFrame::OnLeftDown,    this);
    Bind(wxEVT_MOTION,              &VortchFrame::OnMotion,      this);
    Bind(wxEVT_LEFT_UP,             &VortchFrame::OnLeftUp,      this);
    Bind(wxEVT_ENTER_WINDOW,        &VortchFrame::OnEnter,       this);
    Bind(wxEVT_LEAVE_WINDOW,        &VortchFrame::OnLeave,       this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST,  &VortchFrame::OnCaptureLost, this);
    Bind(wxEVT_TIMER, &VortchFrame::OnEscTimer,   this, ID_TIMER_ESC);
    Bind(wxEVT_TIMER, &VortchFrame::OnAnimTimer,  this, ID_TIMER_ANIM);
    Bind(wxEVT_TIMER, &VortchFrame::OnLeaveTimer, this, ID_TIMER_LEAVE);
#ifdef __WXMSW__
    {
      HWND hwnd = static_cast<HWND>(GetHandle());
      const LONG_PTR ex = ::GetWindowLongPtr(hwnd, GWL_EXSTYLE);
      ::SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex | WS_EX_NOACTIVATE);
    }
#endif
    ApplyZMode();
  }

#ifdef __WXMSW__
  WXLRESULT MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam) override {
    if (msg == WM_WINDOWPOSCHANGING && zmode_ == ZMode::OnDesktop && !moveMode_) {
      auto* wp = reinterpret_cast<WINDOWPOS*>(lParam);
      wp->hwndInsertAfter = HWND_BOTTOM;
      wp->flags &= ~SWP_NOZORDER;
    }
    return wxFrame::MSWWindowProc(msg, wParam, lParam);
  }
#endif

  // ---- drag-drop hooks (called by VortchDropTarget) ----
  void OnDragEnter() {
    dragOver_ = true;
    if (peekMode_ && !moveMode_) AnimateTo(kFullSize);  // drag-to-expand
  }
  void OnDragLeave() {
    dragOver_ = false;
    if (peekMode_ && !moveMode_) leaveTimer_.StartOnce(180);
  }
  void OnFilesDropped(const wxArrayString& files) {
    dragOver_ = false;
    // PLACEHOLDER: real dispatch (classify -> processor) replaces this.
    wxString msg = wxString::Format("Dropped %d item(s):", (int)files.GetCount());
    for (size_t i = 0; i < files.GetCount(); ++i) msg += "\n" + files[i];
    CallAfter([this, msg] {
      wxMessageBox(msg, "vortch (placeholder)", wxOK | wxICON_INFORMATION);
      ReconcilePeek();  // drag-drop + modal box desync enter/leave; re-sync size
    });
  }

private:
  enum {
    ID_MOVE = wxID_HIGHEST + 1, ID_Z_TOPMOST, ID_Z_ONDESKTOP,
    ID_PEEK, ID_ALLDESK,
    ID_TIMER_ESC, ID_TIMER_ANIM, ID_TIMER_LEAVE
  };

  // ---- z-order ----
  void ApplyZMode() {
#ifdef __WXMSW__
    HWND hwnd = static_cast<HWND>(GetHandle());
    if (zmode_ == ZMode::Topmost) {
      ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
      ::SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
      ::SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
#endif
  }

  // ---- peek animation (center-anchored resize) ----
  void ResizeKeepingCenter(int s) {
    const wxRect r = GetRect();
    const wxPoint c(r.x + r.width / 2, r.y + r.height / 2);
    SetSize(c.x - s / 2, c.y - s / 2, s, s);
    Refresh();
  }
  void AnimateTo(int target) {
    targetSize_ = target;
    if (!animTimer_.IsRunning()) animTimer_.Start(16);
  }
  void SnapTo(int s) {
    if (animTimer_.IsRunning()) animTimer_.Stop();
    curSize_ = targetSize_ = s;
    ResizeKeepingCenter(s);
  }
  void OnAnimTimer(wxTimerEvent&) {
    if (curSize_ == targetSize_) { animTimer_.Stop(); return; }
    int diff = targetSize_ - curSize_;
    int step = diff / 4;                       // ease-out
    if (step == 0) step = (diff > 0) ? 1 : -1;
    curSize_ += step;
    if ((diff > 0 && curSize_ > targetSize_) ||
        (diff < 0 && curSize_ < targetSize_)) curSize_ = targetSize_;
    ResizeKeepingCenter(curSize_);
    if (curSize_ == targetSize_) animTimer_.Stop();
  }
  void OnEnter(wxMouseEvent& e) {
    hovered_ = true;
    if (peekMode_ && !moveMode_) AnimateTo(kFullSize);
    e.Skip();
  }
  void OnLeave(wxMouseEvent& e) {
    hovered_ = false;
    if (peekMode_ && !moveMode_) leaveTimer_.StartOnce(180);
    e.Skip();
  }
  void OnLeaveTimer(wxTimerEvent&) {
    if (!peekMode_ || moveMode_) return;
    const bool over = GetScreenRect().Contains(wxGetMousePosition());
    hovered_ = over;
    AnimateTo((over || dragOver_) ? kFullSize : kPeekSize);
  }
  // Reconcile peek size to the actual cursor position (enter/leave events don't
  // fire across a modal popup menu, so a leave during the menu is missed).
  void ReconcilePeek() {
    if (!peekMode_ || moveMode_) return;
    const bool over = GetScreenRect().Contains(wxGetMousePosition());
    hovered_ = over;
    AnimateTo((over || dragOver_) ? kFullSize : kPeekSize);
  }

  // ---- context menu ----
  void OnContextMenu(wxContextMenuEvent&) {
    if (moveMode_) return;
    wxMenu menu;
    menu.Append(ID_MOVE, "Move");
    menu.AppendSeparator();
    auto* top = menu.AppendRadioItem(ID_Z_TOPMOST,   "Always on top");
    auto* des = menu.AppendRadioItem(ID_Z_ONDESKTOP, "Below apps (on desktop)");
    (zmode_ == ZMode::Topmost ? top : des)->Check(true);
    menu.AppendSeparator();
    menu.AppendCheckItem(ID_PEEK, "Peek mode (hover to expand)")->Check(peekMode_);
    auto* all = menu.AppendCheckItem(ID_ALLDESK, "Show on all desktops");
    all->Check(allDesktops_);
#ifdef __WXMSW__
    all->Enable(false);  // deferred on Windows (needs undocumented virtual-desktop API)
#endif
    menu.AppendSeparator();
    menu.Append(wxID_EXIT, "Quit vortch");

    pendingMove_ = false;
#ifdef __WXMSW__
    HWND self   = static_cast<HWND>(GetHandle());
    HWND prevFg = ::GetForegroundWindow();
    ::SetForegroundWindow(self);
    PopupMenu(&menu);
    if (pendingMove_) {
      pendingMove_     = false;
      savedForeground_ = prevFg;
      EnterMoveMode();
    } else if (prevFg && prevFg != self) {
      ::SetForegroundWindow(prevFg);
    }
#else
    PopupMenu(&menu);
    if (pendingMove_) { pendingMove_ = false; EnterMoveMode(); }
#endif
    if (!moveMode_) ReconcilePeek();
  }

  void OnMenu(wxCommandEvent& e) {
    switch (e.GetId()) {
      case ID_MOVE:        pendingMove_ = true; break;  // deferred until menu closes
      case ID_Z_TOPMOST:   zmode_ = ZMode::Topmost;   ApplyZMode(); break;
      case ID_Z_ONDESKTOP: zmode_ = ZMode::OnDesktop; ApplyZMode(); break;
      case ID_ALLDESK:     allDesktops_ = e.IsChecked(); /* TODO platform apply */ break;
      case ID_PEEK:
        peekMode_ = e.IsChecked();
        if (peekMode_) { if (!hovered_ && !moveMode_) AnimateTo(kPeekSize); }
        else AnimateTo(kFullSize);
        break;
      case wxID_EXIT:      wxTheApp->ExitMainLoop(); break;
      default: e.Skip();
    }
  }

  // ---- move mode ----
  void EnterMoveMode() {
    moveMode_    = true;
    dragging_    = false;
    originalPos_ = GetPosition();
    SnapTo(kFullSize);                       // move at full size
    SetCursor(wxCursor(wxCURSOR_SIZING));
    if (!HasCapture()) CaptureMouse();
    escTimer_.Start(25);                      // poll Esc (no reliable focus)
#ifdef __WXMSW__
    ::SetWindowPos(static_cast<HWND>(GetHandle()), HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);  // temp topmost
#else
    Raise();
#endif
    Refresh();
  }

  void ExitMoveMode(bool revert) {
    if (!moveMode_) return;
    escTimer_.Stop();
    if (revert) Move(originalPos_);
    moveMode_ = false;
    dragging_ = false;
    if (HasCapture()) ReleaseMouse();
    SetCursor(wxNullCursor);
    ApplyZMode();
#ifdef __WXMSW__
    if (savedForeground_) {
      HWND fg = static_cast<HWND>(savedForeground_);
      savedForeground_ = nullptr;
      ::SetForegroundWindow(fg);
    }
#endif
    if (peekMode_ && !hovered_) AnimateTo(kPeekSize);
    Refresh();
  }

  void OnEscTimer(wxTimerEvent&) {
    if (moveMode_ && wxGetKeyState(WXK_ESCAPE)) ExitMoveMode(/*revert=*/true);
  }

  void OnLeftDown(wxMouseEvent& e) {
    if (!moveMode_) { e.Skip(); return; }
    const wxPoint mouse = wxGetMousePosition();
    if (GetScreenRect().Contains(mouse)) {
      dragging_       = true;
      dragMouseStart_ = mouse;
      dragWinStart_   = GetPosition();
    } else {
      ExitMoveMode(/*revert=*/false);
    }
  }
  void OnMotion(wxMouseEvent& e) {
    if (moveMode_ && dragging_ && e.Dragging() && e.LeftIsDown()) {
      const wxPoint delta = wxGetMousePosition() - dragMouseStart_;
      Move(dragWinStart_ + delta);
    } else {
      e.Skip();
    }
  }
  void OnLeftUp(wxMouseEvent& e) {
    if (moveMode_ && dragging_) ExitMoveMode(/*revert=*/false);
    else e.Skip();
  }
  void OnCaptureLost(wxMouseCaptureLostEvent&) {
    if (moveMode_) ExitMoveMode(/*revert=*/true);
  }

  void OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();
    const wxSize sz = GetClientSize();
    wxBitmap bmp = bundle_.GetBitmap(sz);
    if (bmp.IsOk()) dc.DrawBitmap(bmp, 0, 0, /*useMask=*/true);
    if (moveMode_) {
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
      dc.SetPen(wxPen(kAccent, 3));
      dc.DrawRectangle(1, 1, sz.GetWidth() - 2, sz.GetHeight() - 2);
    }
  }

  wxBitmapBundle bundle_;
  wxTimer escTimer_, animTimer_, leaveTimer_;
  ZMode   zmode_       = ZMode::Topmost;
  bool    moveMode_    = false;
  bool    dragging_    = false;
  bool    pendingMove_ = false;
  bool    peekMode_    = false;
  bool    allDesktops_ = false;
  bool    hovered_     = false;
  bool    dragOver_    = false;
  int     curSize_     = kFullSize;
  int     targetSize_  = kFullSize;
  WXHWND  savedForeground_ = nullptr;
  wxPoint originalPos_;
  wxPoint dragMouseStart_;
  wxPoint dragWinStart_;
};

// File drop target: drives drag-to-expand (enter/leave) and the drop handler.
class VortchDropTarget : public wxFileDropTarget {
public:
  explicit VortchDropTarget(VortchFrame* f) : frame_(f) {}
  bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override {
    frame_->OnFilesDropped(filenames);
    return true;
  }
  wxDragResult OnEnter(wxCoord, wxCoord, wxDragResult def) override {
    frame_->OnDragEnter();
    return def;
  }
  void OnLeave() override { frame_->OnDragLeave(); }
private:
  VortchFrame* frame_;
};

class VortchApp : public wxApp {
public:
  bool OnInit() override {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.push_back(argv[i].ToStdString());

    const bool fInit    = hasFlag(args, "--init");
    const bool fStartup = hasFlag(args, "--startup");
    const bool fInstall = hasFlag(args, "--install");
    const bool fUninst  = hasFlag(args, "--uninstall");
    const bool fEnable  = hasFlag(args, "--enable-autostart");
    const bool fDisable = hasFlag(args, "--disable-autostart");
    const bool fForce   = hasFlag(args, "--force");
    const bool fNoLaunch= hasFlag(args, "--nolaunch");
    const bool fWelcome = hasFlag(args, "--welcome");

    // Resolve the store location: exe folder by default, or --data-dir.
    const wxString exePath  = wxStandardPaths::Get().GetExecutablePath();
    const std::string ddArg = flagValue(args, "--data-dir");
    wxString dataDir = ddArg.empty() ? wxFileName(exePath).GetPath()
                                     : wxString::FromUTF8(ddArg);
    const std::filesystem::path dbPath =
        vortch::utf8ToPath(dataDir.utf8_string()) / "vortch.db";

    if (!(fInit || fStartup || fInstall || fUninst || fEnable || fDisable)) {
      ShowInfo();
      return false;                                   // bare / double-click
    }

    const bool doInit    = fInit || fInstall;
    const bool doEnable  = fEnable || fInstall;
    const bool doDisable = fDisable || fUninst;

    if (doInit && !InitStore(dbPath, fForce)) return false;

    if (doEnable) {
      std::string cmd = "\"" + std::string(exePath.utf8_string()) + "\" --startup";
      if (!ddArg.empty()) cmd += " --data-dir \"" + ddArg + "\"";
      if (!vortch::installAutostart(cmd))
        wxMessageBox("Could not enable autostart.", "vortch", wxOK | wxICON_WARNING);
    }
    if (doDisable) vortch::uninstallAutostart();

    const bool run = fStartup || (fInstall && !fNoLaunch);
    if (!run) {
      wxString msg = "vortch: done.";
      if (doInit)    msg += "\n  - store initialized";
      if (doEnable)  msg += "\n  - autostart enabled";
      if (doDisable) msg += "\n  - autostart disabled";
      wxMessageBox(msg, "vortch", wxOK | wxICON_INFORMATION);
      return false;
    }

    // ---- run ----
    if (!std::filesystem::exists(dbPath)) {
      wxMessageBox("Not initialized. Run with --install (or --init) first.",
                   "vortch", wxOK | wxICON_ERROR);
      return false;
    }
    try {
      store_ = vortch::Store::open(dbPath);
    } catch (const std::exception& e) {
      wxMessageBox(wxString("Could not open store: ") + e.what(),
                   "vortch", wxOK | wxICON_ERROR);
      return false;
    }

    { vortch::LogEntry e; e.level = "info";
      e.machine = wxGetHostName().utf8_string();
      e.user = wxGetUserId().utf8_string();
      e.body["event"] = "startup";
      store_->appendLog(e); }

    if (fWelcome || store_->getMeta("welcomed").value_or("false") != "true") {
      wxMessageBox("Welcome to vortch!\n(placeholder welcome screen)",
                   "vortch", wxOK | wxICON_INFORMATION);
      store_->setMeta("welcomed", "true");
    }

    // Single hard-coded widget for now; DB-driven instances are the next slice.
    wxBitmapBundle bundle =
        wxBitmapBundle::FromSVGFile(AssetPath("icon.svg"), wxSize(256, 256));
    frame_ = new VortchFrame(bundle);
    frame_->Centre();
    frame_->Show();
    frame_->SetDropTarget(new VortchDropTarget(frame_));
    tray_ = new VortchTray(bundle, frame_);
    return true;
  }

  int OnExit() override {
    if (tray_) { tray_->RemoveIcon(); delete tray_; tray_ = nullptr; }
    return 0;
  }

private:
  void ShowInfo() {
    wxDialog dlg(nullptr, wxID_ANY, "vortch");
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    auto* text  = new wxStaticText(&dlg, wxID_ANY,
        "vortch - a desktop job-launcher drop target.\n\n"
        "It normally starts automatically at login. To set it up:\n\n"
        "    vortch --install      set up + enable autostart + run\n"
        "    vortch --uninstall    remove autostart");
    auto* link  = new wxHyperlinkCtrl(&dlg, wxID_ANY,
        "https://github.com/ziggurat29/vortch",
        "https://github.com/ziggurat29/vortch");
    sizer->Add(text, 0, wxALL, 16);
    sizer->Add(link, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
    sizer->Add(dlg.CreateButtonSizer(wxOK), 0,
               wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    dlg.SetSizerAndFit(sizer);
    dlg.Centre();
    dlg.ShowModal();
  }

  bool InitStore(const std::filesystem::path& dbPath, bool force) {
    if (std::filesystem::exists(dbPath) && !force) {
      wxMessageBox(wxString::FromUTF8(vortch::pathToUtf8(dbPath)) +
                       "\n\nAlready initialized. Use --force to reinitialize.",
                   "vortch", wxOK | wxICON_WARNING);
      return false;
    }
    if (force) { std::error_code ec; std::filesystem::remove(dbPath, ec); }
    try {
      vortch::Store s = vortch::Store::open(dbPath);
      s.setMeta("database_id", vortch::newUuid());
      s.setMeta("created", std::to_string(vortch::nowUnix()));
      s.setMeta("welcomed", "false");
    } catch (const std::exception& e) {
      wxMessageBox(wxString("Init failed: ") + e.what(), "vortch", wxOK | wxICON_ERROR);
      return false;
    }
    return true;
  }

  VortchFrame* frame_ = nullptr;
  VortchTray*  tray_  = nullptr;
  std::optional<vortch::Store> store_;
};

wxIMPLEMENT_APP(VortchApp);
