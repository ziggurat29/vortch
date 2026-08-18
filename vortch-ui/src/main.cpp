#include <wx/wx.h>
#include <wx/bmpbndl.h>
#include <wx/dcbuffer.h>
#include <wx/taskbar.h>
#include <wx/dnd.h>
#include <wx/hyperlink.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <wx/snglinst.h>

#include <optional>
#include <functional>
#include <filesystem>
#include <string>
#include <vector>

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

vortch::StoredObject makeDefaultVortex(const std::string& machine, int x, int y) {
  auto o = vortch::newStoredObject("vortex", "Vortex");
  o.facets["machines"] = nlohmann::json::array({ machine });
  o.body["icon"]   = "vortch:builtin/icon";
  o.body["params"] = nlohmann::json::object();
  nlohmann::json vv;
  vv["position"]["x"] = x;  vv["position"]["y"] = y;
  vv["size"]["w"] = kFullSize;  vv["size"]["h"] = kFullSize;
  vv["zmode"] = "topmost";
  vv["peek"]  = false;
  o.body["visual"]["vortex"] = vv;
  return o;
}
} // namespace

// The app implements this so frames/tray can act on it without a hard dependency
// on the concrete VortchApp (which is defined last).
struct VortchController {
  virtual ~VortchController() = default;
  virtual void saveVortexVisual(const std::string& id, const nlohmann::json& v) = 0;
  virtual void removeVortex(const std::string& id) = 0;
  virtual void newVortex() = 0;
  virtual void toggleAllVisible() = 0;
  virtual bool anyVisible() = 0;
  virtual bool hasVortices() = 0;
};

// Tray icon: create/show-hide vortices + quit.
class VortchTray : public wxTaskBarIcon {
public:
  VortchTray(const wxBitmapBundle& bundle, VortchController* ctrl) : ctrl_(ctrl) {
    wxIcon icon;
    icon.CopyFromBitmap(bundle.GetBitmap(wxSize(32, 32)));
    SetIcon(icon, "vortch");
    Bind(wxEVT_MENU, &VortchTray::OnMenu, this);
  }
  wxMenu* CreatePopupMenu() override {
    auto* m = new wxMenu();
    m->Append(ID_NEW, "New vortex");
    auto* toggle = m->Append(ID_TOGGLE,
                             ctrl_->anyVisible() ? "Hide vortices" : "Show vortices");
    if (!ctrl_->hasVortices()) toggle->Enable(false);
    m->AppendSeparator();
    m->Append(wxID_EXIT, "Quit vortch");
    return m;
  }
private:
  enum { ID_NEW = wxID_HIGHEST + 200, ID_TOGGLE };
  void OnMenu(wxCommandEvent& e) {
    switch (e.GetId()) {
      case ID_NEW:     ctrl_->newVortex(); break;
      case ID_TOGGLE:  ctrl_->toggleAllVisible(); break;
      case wxID_EXIT:  wxTheApp->ExitMainLoop(); break;
    }
  }
  VortchController* ctrl_;
};

enum class ZMode { Topmost, OnDesktop };

// Borderless, always-on-top desktop-icon-style vortex widget.
class VortchFrame : public wxFrame {
public:
  VortchFrame(VortchController* ctrl, std::string id,
              const nlohmann::json& visual, wxBitmapBundle bundle)
      : wxFrame(nullptr, wxID_ANY, "vortch", wxDefaultPosition,
                wxSize(kFullSize, kFullSize),
                wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP | wxBORDER_NONE),
        ctrl_(ctrl), id_(std::move(id)), bundle_(std::move(bundle)) {
    SetClientSize(kFullSize, kFullSize);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    escTimer_.SetOwner(this, ID_TIMER_ESC);
    animTimer_.SetOwner(this, ID_TIMER_ANIM);
    leaveTimer_.SetOwner(this, ID_TIMER_LEAVE);
    Bind(wxEVT_PAINT,              &VortchFrame::OnPaint,       this);
    Bind(wxEVT_CONTEXT_MENU,       &VortchFrame::OnContextMenu, this);
    Bind(wxEVT_MENU,               &VortchFrame::OnMenu,        this);
    Bind(wxEVT_LEFT_DOWN,          &VortchFrame::OnLeftDown,    this);
    Bind(wxEVT_MOTION,             &VortchFrame::OnMotion,      this);
    Bind(wxEVT_LEFT_UP,            &VortchFrame::OnLeftUp,      this);
    Bind(wxEVT_ENTER_WINDOW,       &VortchFrame::OnEnter,       this);
    Bind(wxEVT_LEAVE_WINDOW,       &VortchFrame::OnLeave,       this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &VortchFrame::OnCaptureLost, this);
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
    // apply persisted visual.vortex
    zmode_    = (visual.value("zmode", std::string("topmost")) == "onDesktop")
                    ? ZMode::OnDesktop : ZMode::Topmost;
    peekMode_ = visual.value("peek", false);
    if (visual.contains("position") && visual["position"].is_object()) {
      Move(visual["position"].value("x", 0), visual["position"].value("y", 0));
    } else {
      Centre();
    }
    ApplyZMode();
    if (peekMode_) SnapTo(kPeekSize);
  }

  const std::string& objectId() const { return id_; }

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

  // drag-drop hooks (called by VortchDropTarget)
  void OnDragEnter() {
    dragOver_ = true;
    if (peekMode_ && !moveMode_) AnimateTo(kFullSize);
  }
  void OnDragLeave() {
    dragOver_ = false;
    if (peekMode_ && !moveMode_) leaveTimer_.StartOnce(180);
  }
  void OnFilesDropped(const wxArrayString& files) {
    dragOver_ = false;
    wxString msg = wxString::Format("Dropped %d item(s):", (int)files.GetCount());
    for (size_t i = 0; i < files.GetCount(); ++i) msg += "\n" + files[i];
    CallAfter([this, msg] {
      wxMessageBox(msg, "vortch (placeholder)", wxOK | wxICON_INFORMATION);
      ReconcilePeek();
    });
  }

private:
  enum {
    ID_MOVE = wxID_HIGHEST + 1, ID_Z_TOPMOST, ID_Z_ONDESKTOP,
    ID_PEEK, ID_REMOVE,
    ID_TIMER_ESC, ID_TIMER_ANIM, ID_TIMER_LEAVE
  };

  void Save() {
    if (ctrl_ && !id_.empty()) ctrl_->saveVortexVisual(id_, CurrentVisual());
  }
  nlohmann::json CurrentVisual() {
    const wxRect r = GetRect();
    const wxPoint c(r.x + r.width / 2, r.y + r.height / 2);  // stable center
    nlohmann::json v;
    v["position"]["x"] = c.x - kFullSize / 2;
    v["position"]["y"] = c.y - kFullSize / 2;
    v["size"]["w"] = kFullSize;  v["size"]["h"] = kFullSize;
    v["zmode"] = (zmode_ == ZMode::OnDesktop) ? "onDesktop" : "topmost";
    v["peek"]  = peekMode_;
    return v;
  }

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

  // peek animation (center-anchored resize)
  void ResizeKeepingCenter(int s) {
    const wxRect r = GetRect();
    const wxPoint c(r.x + r.width / 2, r.y + r.height / 2);
    SetSize(c.x - s / 2, c.y - s / 2, s, s);
    Refresh();
  }
  void AnimateTo(int target) { targetSize_ = target; if (!animTimer_.IsRunning()) animTimer_.Start(16); }
  void SnapTo(int s) { if (animTimer_.IsRunning()) animTimer_.Stop(); curSize_ = targetSize_ = s; ResizeKeepingCenter(s); }
  void OnAnimTimer(wxTimerEvent&) {
    if (curSize_ == targetSize_) { animTimer_.Stop(); return; }
    int diff = targetSize_ - curSize_, step = diff / 4;
    if (step == 0) step = (diff > 0) ? 1 : -1;
    curSize_ += step;
    if ((diff > 0 && curSize_ > targetSize_) || (diff < 0 && curSize_ < targetSize_)) curSize_ = targetSize_;
    ResizeKeepingCenter(curSize_);
    if (curSize_ == targetSize_) animTimer_.Stop();
  }
  void OnEnter(wxMouseEvent& e) { hovered_ = true;  if (peekMode_ && !moveMode_) AnimateTo(kFullSize); e.Skip(); }
  void OnLeave(wxMouseEvent& e) { hovered_ = false; if (peekMode_ && !moveMode_) leaveTimer_.StartOnce(180); e.Skip(); }
  void OnLeaveTimer(wxTimerEvent&) {
    if (!peekMode_ || moveMode_) return;
    const bool over = GetScreenRect().Contains(wxGetMousePosition());
    hovered_ = over;
    AnimateTo((over || dragOver_) ? kFullSize : kPeekSize);
  }
  void ReconcilePeek() {
    if (!peekMode_ || moveMode_) return;
    const bool over = GetScreenRect().Contains(wxGetMousePosition());
    hovered_ = over;
    AnimateTo((over || dragOver_) ? kFullSize : kPeekSize);
  }

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
    menu.AppendSeparator();
    menu.Append(ID_REMOVE, "Remove this vortex");
    menu.Append(wxID_EXIT, "Quit vortch");

    pendingMove_ = false;
#ifdef __WXMSW__
    HWND self   = static_cast<HWND>(GetHandle());
    HWND prevFg = ::GetForegroundWindow();
    ::SetForegroundWindow(self);
    PopupMenu(&menu);
    if (pendingMove_) { pendingMove_ = false; savedForeground_ = prevFg; EnterMoveMode(); }
    else if (prevFg && prevFg != self) ::SetForegroundWindow(prevFg);
#else
    PopupMenu(&menu);
    if (pendingMove_) { pendingMove_ = false; EnterMoveMode(); }
#endif
    if (!moveMode_) ReconcilePeek();
  }

  void OnMenu(wxCommandEvent& e) {
    switch (e.GetId()) {
      case ID_MOVE:        pendingMove_ = true; break;
      case ID_Z_TOPMOST:   zmode_ = ZMode::Topmost;   ApplyZMode(); Save(); break;
      case ID_Z_ONDESKTOP: zmode_ = ZMode::OnDesktop; ApplyZMode(); Save(); break;
      case ID_PEEK:
        peekMode_ = e.IsChecked();
        if (peekMode_) { if (!hovered_ && !moveMode_) AnimateTo(kPeekSize); }
        else AnimateTo(kFullSize);
        Save();
        break;
      case ID_REMOVE:      if (ctrl_) ctrl_->removeVortex(id_); break;  // destroys this
      case wxID_EXIT:      wxTheApp->ExitMainLoop(); break;
      default: e.Skip();
    }
  }

  void EnterMoveMode() {
    moveMode_ = true; dragging_ = false;
    originalPos_ = GetPosition();
    SnapTo(kFullSize);
    SetCursor(wxCursor(wxCURSOR_SIZING));
    if (!HasCapture()) CaptureMouse();
    escTimer_.Start(25);
#ifdef __WXMSW__
    ::SetWindowPos(static_cast<HWND>(GetHandle()), HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
    Raise();
#endif
    Refresh();
  }
  void ExitMoveMode(bool revert) {
    if (!moveMode_) return;
    escTimer_.Stop();
    if (revert) Move(originalPos_);
    moveMode_ = false; dragging_ = false;
    if (HasCapture()) ReleaseMouse();
    SetCursor(wxNullCursor);
    ApplyZMode();
#ifdef __WXMSW__
    if (savedForeground_) { HWND fg = static_cast<HWND>(savedForeground_); savedForeground_ = nullptr; ::SetForegroundWindow(fg); }
#endif
    if (peekMode_ && !hovered_) AnimateTo(kPeekSize);
    Refresh();
  }
  void OnEscTimer(wxTimerEvent&) { if (moveMode_ && wxGetKeyState(WXK_ESCAPE)) ExitMoveMode(true); }
  void OnLeftDown(wxMouseEvent& e) {
    if (!moveMode_) { e.Skip(); return; }
    const wxPoint mouse = wxGetMousePosition();
    if (GetScreenRect().Contains(mouse)) { dragging_ = true; dragMouseStart_ = mouse; dragWinStart_ = GetPosition(); }
    else ExitMoveMode(false);
  }
  void OnMotion(wxMouseEvent& e) {
    if (moveMode_ && dragging_ && e.Dragging() && e.LeftIsDown())
      Move(dragWinStart_ + (wxGetMousePosition() - dragMouseStart_));
    else e.Skip();
  }
  void OnLeftUp(wxMouseEvent& e) {
    if (moveMode_ && dragging_) { ExitMoveMode(false); Save(); }  // commit + persist
    else e.Skip();
  }
  void OnCaptureLost(wxMouseCaptureLostEvent&) { if (moveMode_) ExitMoveMode(true); }

  void OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();
    const wxSize sz = GetClientSize();
    wxBitmap bmp = bundle_.GetBitmap(sz);
    if (bmp.IsOk()) dc.DrawBitmap(bmp, 0, 0, true);
    if (moveMode_) {
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
      dc.SetPen(wxPen(kAccent, 3));
      dc.DrawRectangle(1, 1, sz.GetWidth() - 2, sz.GetHeight() - 2);
    }
  }

  VortchController* ctrl_;
  std::string       id_;
  wxBitmapBundle    bundle_;
  wxTimer escTimer_, animTimer_, leaveTimer_;
  ZMode   zmode_       = ZMode::Topmost;
  bool    moveMode_    = false;
  bool    dragging_    = false;
  bool    pendingMove_ = false;
  bool    peekMode_    = false;
  bool    hovered_     = false;
  bool    dragOver_    = false;
  int     curSize_     = kFullSize;
  int     targetSize_  = kFullSize;
  void*   savedForeground_ = nullptr;  // HWND on MSW; only used under __WXMSW__
  wxPoint originalPos_, dragMouseStart_, dragWinStart_;
};

// File drop target: drives drag-to-expand + the drop handler.
class VortchDropTarget : public wxFileDropTarget {
public:
  explicit VortchDropTarget(VortchFrame* f) : frame_(f) {}
  bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override { frame_->OnFilesDropped(filenames); return true; }
  wxDragResult OnEnter(wxCoord, wxCoord, wxDragResult def) override { frame_->OnDragEnter(); return def; }
  void OnLeave() override { frame_->OnDragLeave(); }
private:
  VortchFrame* frame_;
};

class VortchApp : public wxApp, public VortchController {
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

    const wxString exePath  = wxStandardPaths::Get().GetExecutablePath();
    const std::string ddArg = flagValue(args, "--data-dir");
    wxString dataDir = ddArg.empty() ? wxFileName(exePath).GetPath()
                                     : wxString::FromUTF8(ddArg);
    const std::filesystem::path dbPath =
        vortch::utf8ToPath(dataDir.utf8_string()) / "vortch.db";

    if (!(fInit || fStartup || fInstall || fUninst || fEnable || fDisable)) { ShowInfo(); return false; }

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
    // Single instance per store: two processes on one DB clobber each other.
    instanceChecker_.Create(wxString::Format(
        "vortch-%zu", std::hash<std::string>{}(vortch::pathToUtf8(dbPath))));
    if (instanceChecker_.IsAnotherRunning()) {
      wxMessageBox("vortch is already running for this store.",
                   "vortch", wxOK | wxICON_INFORMATION);
      return false;
    }
    if (!std::filesystem::exists(dbPath)) {
      wxMessageBox("Not initialized. Run with --install (or --init) first.", "vortch", wxOK | wxICON_ERROR);
      return false;
    }
    try { store_ = vortch::Store::open(dbPath); }
    catch (const std::exception& e) {
      wxMessageBox(wxString("Could not open store: ") + e.what(), "vortch", wxOK | wxICON_ERROR);
      return false;
    }
    machine_ = wxGetHostName().utf8_string();

    { vortch::LogEntry e; e.level = "info"; e.machine = machine_;
      e.user = wxGetUserId().utf8_string(); e.body["event"] = "startup";
      store_->appendLog(e); }

    if (fWelcome || store_->getMeta("welcomed").value_or("false") != "true") {
      wxMessageBox("Welcome to vortch!\n(placeholder welcome screen)", "vortch", wxOK | wxICON_INFORMATION);
      store_->setMeta("welcomed", "true");
    }

    SetExitOnFrameDelete(false);  // keep running with only a tray (zero vortices)
    bundle_ = wxBitmapBundle::FromSVGFile(AssetPath("icon.svg"), wxSize(256, 256));
    LoadVortices();
    return true;
  }

  int OnExit() override {
    if (tray_) { tray_->RemoveIcon(); delete tray_; tray_ = nullptr; }
    return 0;
  }

  // ---- VortchController ----
  void saveVortexVisual(const std::string& id, const nlohmann::json& v) override {
    if (!store_) return;
    auto obj = store_->getObject(id);
    if (!obj) return;
    obj->body["visual"]["vortex"] = v;
    obj->modified = vortch::nowUnix();
    store_->putObject(*obj);
  }
  void removeVortex(const std::string& id) override {
    if (wxMessageBox("Remove this vortex?", "vortch", wxYES_NO | wxICON_QUESTION) != wxYES) return;
    if (store_) store_->removeObject(id);
    for (auto it = frames_.begin(); it != frames_.end(); ++it) {
      if ((*it)->objectId() == id) { (*it)->Destroy(); frames_.erase(it); break; }
    }
  }
  void newVortex() override {
    if (!store_) return;
    const wxSize scr = wxGetDisplaySize();
    const int n = static_cast<int>(frames_.size());
    const int x = scr.GetWidth() / 2 - kFullSize / 2 + n * 36;
    const int y = scr.GetHeight() / 2 - kFullSize / 2 + n * 36;
    auto o = makeDefaultVortex(machine_, x, y);
    store_->putObject(o);
    CreateFrame(o);
  }
  void toggleAllVisible() override {
    const bool anyShown = anyVisible();
    for (auto* f : frames_) f->Show(!anyShown);
  }
  bool anyVisible() override {
    for (auto* f : frames_) if (f->IsShown()) return true;
    return false;
  }
  bool hasVortices() override { return !frames_.empty(); }

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
        "https://github.com/ziggurat29/vortch", "https://github.com/ziggurat29/vortch");
    sizer->Add(text, 0, wxALL, 16);
    sizer->Add(link, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
    sizer->Add(dlg.CreateButtonSizer(wxOK), 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 12);
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
      // freebie: one default vortex
      const wxSize scr = wxGetDisplaySize();
      s.putObject(makeDefaultVortex(wxGetHostName().utf8_string(),
                                    scr.GetWidth() / 2 - kFullSize / 2,
                                    scr.GetHeight() / 2 - kFullSize / 2));
    } catch (const std::exception& e) {
      wxMessageBox(wxString("Init failed: ") + e.what(), "vortch", wxOK | wxICON_ERROR);
      return false;
    }
    return true;
  }

  void CreateFrame(const vortch::StoredObject& o) {
    nlohmann::json visual =
        o.body.value("visual", nlohmann::json::object()).value("vortex", nlohmann::json::object());
    auto* f = new VortchFrame(this, o.id, visual, bundle_);
    f->Show();
    f->SetDropTarget(new VortchDropTarget(f));
    frames_.push_back(f);
  }

  void LoadVortices() {
    // No auto-seed here: the freebie vortex is created ONLY at --init. If the
    // user removes every vortex, --startup shows none (create via the tray).
    for (auto& o : store_->queryByFacet("vortex", "machines", machine_)) CreateFrame(o);
    tray_ = new VortchTray(bundle_, this);
  }

  std::optional<vortch::Store> store_;
  std::string                  machine_;
  wxBitmapBundle               bundle_;
  std::vector<VortchFrame*>    frames_;
  VortchTray*                  tray_ = nullptr;
  wxSingleInstanceChecker      instanceChecker_;
};

wxIMPLEMENT_APP(VortchApp);
