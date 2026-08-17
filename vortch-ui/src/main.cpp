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
} // namespace

// Tray icon with a minimal menu (Quit). Reachability backstop when the widget
// window is hidden/collapsed.
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

// Borderless, always-on-top desktop-icon-style window (Tier C).
class VortchFrame : public wxFrame {
public:
  explicit VortchFrame(wxBitmapBundle bundle)
      : wxFrame(nullptr, wxID_ANY, "vortch", wxDefaultPosition, wxSize(96, 96),
                wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP | wxBORDER_NONE),
        bundle_(std::move(bundle)) {
    SetClientSize(96, 96);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &VortchFrame::OnPaint, this);
  }

private:
  void OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();
    const wxSize sz = GetClientSize();
    wxBitmap bmp = bundle_.GetBitmap(sz);
    if (bmp.IsOk()) dc.DrawBitmap(bmp, 0, 0, /*useMask=*/true);
  }

  wxBitmapBundle bundle_;
};

class VortchApp : public wxApp {
public:
  bool OnInit() override {
    // Demonstrate core linkage: honor a launch identity if present (no-op yet).
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
  VortchTray* tray_ = nullptr;
};

wxIMPLEMENT_APP(VortchApp);
