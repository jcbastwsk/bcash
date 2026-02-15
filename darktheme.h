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

#endif // DARKTHEME_H
