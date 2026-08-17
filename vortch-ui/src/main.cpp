#include <wx/wx.h>
#include <wx/bmpbndl.h>
#include <wx/dcbuffer.h>
#include <wx/taskbar.h>

#include <string>
#include <vector>

#include "config.hpp"
#include "platform.hpp"
#include "vortch/launch.hpp"

namespace {
wxString AssetPath(const char* name) {
  return wxString::FromUTF8(VORTCH_ASSETS_DIR) + "/" + name;
}
const wxColour kAccent(49, 196, 255);  // move-mode highlight
} // namespace

// Tray icon with a minimal menu (Quit).
class VortchTray : public wxTaskBarIcon {
public:
  explicit VortchTray(const wxBitmapBundle& bundle) {
    wxIcon icon;
    icon.CopyFromBitmap(bundle.GetBitmap(wxSize(32, 32)));
    SetIcon(icon, "vortch");
    Bind(wxEVT_MENU, &VortchTray::OnMenu, this);
  }
  wxMenu* CreatePopupMenu() override {
    auto* menu = new wxMenu();
    menu->Append(wxID_EXIT, "Quit vortch");
    return menu;
  }
private:
  void OnMenu(wxCommandEvent& e) {
    if (e.GetId() == wxID_EXIT) wxTheApp->ExitMainLoop();
  }
};

enum class ZMode { Topmost, OnDesktop };

// Borderless, always-on-top desktop-icon-style window (Tier C), with a context
// menu, a "move mode" for repositioning, and a switchable z-order.
class VortchFrame : public wxFrame {
public:
  explicit VortchFrame(wxBitmapBundle bundle)
      : wxFrame(nullptr, wxID_ANY, "vortch", wxDefaultPosition, wxSize(96, 96),
                wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP | wxBORDER_NONE),
        bundle_(std::move(bundle)) {
    SetClientSize(96, 96);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    moveTimer_.SetOwner(this);
    Bind(wxEVT_PAINT,              &VortchFrame::OnPaint,       this);
    Bind(wxEVT_CONTEXT_MENU,       &VortchFrame::OnContextMenu, this);
    Bind(wxEVT_MENU,               &VortchFrame::OnMenu,        this);
    Bind(wxEVT_LEFT_DOWN,          &VortchFrame::OnLeftDown,    this);
    Bind(wxEVT_MOTION,             &VortchFrame::OnMotion,      this);
    Bind(wxEVT_LEFT_UP,            &VortchFrame::OnLeftUp,      this);
    Bind(wxEVT_TIMER,              &VortchFrame::OnMoveTimer,   this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &VortchFrame::OnCaptureLost, this);
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

private:
  enum { ID_MOVE = wxID_HIGHEST + 1, ID_Z_TOPMOST, ID_Z_ONDESKTOP };

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

  void OnContextMenu(wxContextMenuEvent&) {
    if (moveMode_) return;
    wxMenu menu;
    menu.Append(ID_MOVE, "Move");
    menu.AppendSeparator();
    auto* top = menu.AppendRadioItem(ID_Z_TOPMOST,   "Always on top");
    auto* des = menu.AppendRadioItem(ID_Z_ONDESKTOP, "Below apps (on desktop)");
    (zmode_ == ZMode::Topmost ? top : des)->Check(true);
    menu.AppendSeparator();
    menu.Append(wxID_EXIT, "Quit vortch");
    PopupMenu(&menu);
  }

  void OnMenu(wxCommandEvent& e) {
    switch (e.GetId()) {
      case ID_MOVE:        EnterMoveMode(); break;
      case ID_Z_TOPMOST:   zmode_ = ZMode::Topmost;   ApplyZMode(); break;
      case ID_Z_ONDESKTOP: zmode_ = ZMode::OnDesktop; ApplyZMode(); break;
      case wxID_EXIT:      wxTheApp->ExitMainLoop();   break;
      default: e.Skip();
    }
  }

  void EnterMoveMode() {
    moveMode_    = true;
    dragging_    = false;
    originalPos_ = GetPosition();
    SetCursor(wxCursor(wxCURSOR_SIZING));
    if (!HasCapture()) CaptureMouse();
    SetFocus();
    moveTimer_.Start(25);  // poll Esc (borderless window can't rely on focus)
#ifdef __WXMSW__
    // Temporarily float on top while moving, regardless of z-mode.
    ::SetWindowPos(static_cast<HWND>(GetHandle()), HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
    Raise();
#endif
    Refresh();
  }

  void ExitMoveMode(bool revert) {
    if (!moveMode_) return;
    moveTimer_.Stop();
    if (revert) Move(originalPos_);
    moveMode_ = false;
    dragging_ = false;
    if (HasCapture()) ReleaseMouse();
    SetCursor(wxNullCursor);
    ApplyZMode();  // re-assert z-order after moving
    Refresh();
  }

  void OnMoveTimer(wxTimerEvent&) {
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
      ExitMoveMode(/*revert=*/false);  // click off-window cancels
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
    if (moveMode_ && dragging_) {
      ExitMoveMode(/*revert=*/false);  // release commits
    } else {
      e.Skip();
    }
  }

  void OnCaptureLost(wxMouseCaptureLostEvent&) {
    if (moveMode_) {
      moveTimer_.Stop();
      Move(originalPos_);
      moveMode_ = false;
      dragging_ = false;
      SetCursor(wxNullCursor);
      Refresh();
    }
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
  wxTimer moveTimer_;
  ZMode   zmode_    = ZMode::Topmost;
  bool    moveMode_ = false;
  bool    dragging_ = false;
  wxPoint originalPos_;
  wxPoint dragMouseStart_;
  wxPoint dragWinStart_;
};

class VortchApp : public wxApp {
public:
  bool OnInit() override {
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) args.push_back(argv[i].ToStdString());
    (void)vortch::parseVortchId(args);

    wxBitmapBundle bundle =
        wxBitmapBundle::FromSVGFile(AssetPath("icon.svg"), wxSize(256, 256));

    frame_ = new VortchFrame(bundle);
    frame_->Centre();
    frame_->Show();

    tray_ = new VortchTray(bundle);
    return true;
  }

  int OnExit() override {
    if (tray_) {
      tray_->RemoveIcon();
      delete tray_;
      tray_ = nullptr;
    }
    return 0;
  }

private:
  VortchFrame* frame_ = nullptr;
  VortchTray*  tray_  = nullptr;
};

wxIMPLEMENT_APP(VortchApp);
