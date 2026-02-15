// Dark theme colors and utilities for bcash GUI
#ifndef DARKTHEME_H
#define DARKTHEME_H

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/html/htmlwin.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/treectrl.h>
#include <wx/combobox.h>

// Core palette
static const wxColour DARK_BG(0x1a, 0x1a, 0x1a);         // #1a1a1a
static const wxColour DARK_BG_SECONDARY(0x2a, 0x2a, 0x2a); // #2a2a2a
static const wxColour DARK_BG_INPUT(0x22, 0x22, 0x22);    // slightly lighter for inputs
static const wxColour DARK_TEXT(0xe0, 0xe0, 0xe0);        // #e0e0e0
static const wxColour DARK_TEXT_DIM(0x88, 0x88, 0x88);    // dimmed text
static const wxColour DARK_ACCENT(0xFF, 0xD7, 0x00);     // #FFD700 amber/gold
static const wxColour DARK_BORDER(0x3a, 0x3a, 0x3a);     // subtle borders
static const wxColour DARK_SELECTION(0x44, 0x44, 0x00);   // dark gold selection
static const wxColour DARK_BUTTON_BG(0x33, 0x33, 0x33);  // button background
static const wxColour DARK_STATUSBAR(0x11, 0x11, 0x11);   // darker statusbar

// Recursively apply dark theme to a window and all children
inline void ApplyDarkTheme(wxWindow* window)
{
    if (!window) return;

    window->SetBackgroundColour(DARK_BG);
    window->SetForegroundColour(DARK_TEXT);

    // Specific widget types get special treatment
    if (wxTextCtrl* tc = dynamic_cast<wxTextCtrl*>(window)) {
        tc->SetBackgroundColour(DARK_BG_INPUT);
        tc->SetForegroundColour(DARK_TEXT);
    }
    else if (wxListCtrl* lc = dynamic_cast<wxListCtrl*>(window)) {
        lc->SetBackgroundColour(DARK_BG_SECONDARY);
        lc->SetForegroundColour(DARK_TEXT);
        lc->SetTextColour(DARK_TEXT);
    }
    else if (wxNotebook* nb = dynamic_cast<wxNotebook*>(window)) {
        nb->SetBackgroundColour(DARK_BG);
        nb->SetForegroundColour(DARK_TEXT);
    }
    else if (wxButton* btn = dynamic_cast<wxButton*>(window)) {
        btn->SetBackgroundColour(DARK_BUTTON_BG);
        btn->SetForegroundColour(DARK_TEXT);
    }
    else if (wxStatusBar* sb = dynamic_cast<wxStatusBar*>(window)) {
        sb->SetBackgroundColour(DARK_STATUSBAR);
        sb->SetForegroundColour(DARK_TEXT);
    }
    else if (wxMenuBar* mb = dynamic_cast<wxMenuBar*>(window)) {
        mb->SetBackgroundColour(DARK_BG_SECONDARY);
        mb->SetForegroundColour(DARK_TEXT);
    }
    else if (wxHtmlWindow* hw = dynamic_cast<wxHtmlWindow*>(window)) {
        hw->SetBackgroundColour(DARK_BG_SECONDARY);
        hw->SetForegroundColour(DARK_TEXT);
    }
    else if (wxTreeCtrl* tree = dynamic_cast<wxTreeCtrl*>(window)) {
        tree->SetBackgroundColour(DARK_BG_SECONDARY);
        tree->SetForegroundColour(DARK_TEXT);
    }
    else if (wxComboBox* cb = dynamic_cast<wxComboBox*>(window)) {
        cb->SetBackgroundColour(DARK_BG_INPUT);
        cb->SetForegroundColour(DARK_TEXT);
    }
    else if (wxChoice* ch = dynamic_cast<wxChoice*>(window)) {
        ch->SetBackgroundColour(DARK_BG_INPUT);
        ch->SetForegroundColour(DARK_TEXT);
    }
    else if (wxPanel* panel = dynamic_cast<wxPanel*>(window)) {
        panel->SetBackgroundColour(DARK_BG);
        panel->SetForegroundColour(DARK_TEXT);
    }

    // Recurse into children
    wxWindowList& children = window->GetChildren();
    for (wxWindowList::iterator it = children.begin(); it != children.end(); ++it) {
        ApplyDarkTheme(*it);
    }
}

// Apply to a top-level frame (includes menubar)
inline void ApplyDarkThemeToFrame(wxFrame* frame)
{
    if (!frame) return;
    ApplyDarkTheme(frame);
    if (wxMenuBar* mb = frame->GetMenuBar()) {
        mb->SetBackgroundColour(DARK_BG_SECONDARY);
        mb->SetForegroundColour(DARK_TEXT);
    }
    frame->Refresh();
}

// Apply to a dialog
inline void ApplyDarkThemeToDialog(wxDialog* dialog)
{
    if (!dialog) return;
    ApplyDarkTheme(dialog);
    dialog->Refresh();
}


// --- Owner-drawn dark button for macOS (wxButton ignores SetBackgroundColour) ---
#include <wx/dcbuffer.h>

class DarkButton : public wxWindow
{
public:
    DarkButton(wxWindow* parent, wxWindowID id, const wxString& label,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0)
        : wxWindow(parent, id, pos, size, wxBORDER_NONE | wxWANTS_CHARS),
          m_label(label), m_hover(false), m_pressed(false), m_enabled(true),
          m_exactFit(style & wxBU_EXACTFIT)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        // Strip & accelerator prefix for display
        m_displayLabel = label;
        m_displayLabel.Replace("&", "");

        if (size == wxDefaultSize || (size.GetWidth() == -1 && size.GetHeight() == -1)) {
            wxClientDC dc(this);
            dc.SetFont(GetFont().IsOk() ? GetFont() : *wxNORMAL_FONT);
            wxSize textSize = dc.GetTextExtent(m_displayLabel);
            int w = textSize.GetWidth() + (m_exactFit ? 16 : 24);
            int h = textSize.GetHeight() + 12;
            if (!m_exactFit && w < 85) w = 85;
            if (h < 25) h = 25;
            SetMinSize(wxSize(w, h));
            SetSize(wxSize(w, h));
        } else {
            // Respect explicit sizes but fill in -1 dimensions
            wxClientDC dc(this);
            dc.SetFont(GetFont().IsOk() ? GetFont() : *wxNORMAL_FONT);
            wxSize textSize = dc.GetTextExtent(m_displayLabel);
            int w = size.GetWidth() == -1 ? textSize.GetWidth() + 24 : size.GetWidth();
            int h = size.GetHeight() == -1 ? textSize.GetHeight() + 12 : size.GetHeight();
            SetMinSize(wxSize(w, h));
            SetSize(wxSize(w, h));
        }

        Bind(wxEVT_PAINT, &DarkButton::OnPaint, this);
        Bind(wxEVT_ENTER_WINDOW, &DarkButton::OnEnter, this);
        Bind(wxEVT_LEAVE_WINDOW, &DarkButton::OnLeave, this);
        Bind(wxEVT_LEFT_DOWN, &DarkButton::OnMouseDown, this);
        Bind(wxEVT_LEFT_UP, &DarkButton::OnMouseUp, this);
        Bind(wxEVT_KEY_DOWN, &DarkButton::OnKeyDown, this);
    }

    void SetLabel(const wxString& label) override {
        m_label = label;
        m_displayLabel = label;
        m_displayLabel.Replace("&", "");
        Refresh();
    }
    wxString GetLabel() const override { return m_label; }

    bool Enable(bool enable = true) override {
        m_enabled = enable;
        wxWindow::Enable(enable);
        Refresh();
        return true;
    }

private:
    wxString m_label;
    wxString m_displayLabel;
    bool m_hover, m_pressed, m_enabled, m_exactFit;

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        wxSize sz = GetClientSize();
        wxRect rect(0, 0, sz.x, sz.y);

        // Background
        wxColour bg;
        if (!m_enabled)
            bg = wxColour(0x25, 0x25, 0x25);
        else if (m_pressed)
            bg = wxColour(0x50, 0x48, 0x00); // dark gold press
        else if (m_hover)
            bg = wxColour(0x3a, 0x3a, 0x3a);
        else
            bg = DARK_BUTTON_BG;

        dc.SetBrush(wxBrush(bg));
        // Border color
        wxColour border = m_hover && m_enabled ? DARK_ACCENT : DARK_BORDER;
        dc.SetPen(wxPen(border, 1));

        // Rounded rect
        dc.DrawRoundedRectangle(rect, 4);

        // Text
        wxColour textCol = m_enabled ? DARK_TEXT : DARK_TEXT_DIM;
        dc.SetTextForeground(textCol);
        dc.SetFont(GetFont().IsOk() ? GetFont() : *wxNORMAL_FONT);
        wxSize textSz = dc.GetTextExtent(m_displayLabel);
        int tx = (sz.x - textSz.x) / 2;
        int ty = (sz.y - textSz.y) / 2;
        dc.DrawText(m_displayLabel, tx, ty);
    }

    void OnEnter(wxMouseEvent&) { m_hover = true; Refresh(); }
    void OnLeave(wxMouseEvent&) { m_hover = false; m_pressed = false; Refresh(); }
    void OnMouseDown(wxMouseEvent&) {
        if (!m_enabled) return;
        m_pressed = true; Refresh(); CaptureMouse();
    }
    void OnMouseUp(wxMouseEvent& evt) {
        if (HasCapture()) ReleaseMouse();
        if (!m_enabled) return;
        m_pressed = false; Refresh();
        wxRect r(GetClientSize());
        if (r.Contains(evt.GetPosition())) {
            wxCommandEvent cmdEvt(wxEVT_BUTTON, GetId());
            cmdEvt.SetEventObject(this);
            ProcessWindowEvent(cmdEvt);
        }
    }
    void OnKeyDown(wxKeyEvent& evt) {
        if (evt.GetKeyCode() == WXK_RETURN || evt.GetKeyCode() == WXK_SPACE) {
            wxCommandEvent cmdEvt(wxEVT_BUTTON, GetId());
            cmdEvt.SetEventObject(this);
            ProcessWindowEvent(cmdEvt);
        } else {
            evt.Skip();
        }
    }
};

#endif // DARKTHEME_H
