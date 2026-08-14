#pragma once

#include "main.h"

#include <windows.h>

using PopupCommandHandler = void (*)(void*, UINT);

constexpr UINT ID_POPUP_SIGN_IN = 101;
constexpr UINT ID_POPUP_CHECK = 102;
constexpr UINT ID_POPUP_REFRESH = 103;
constexpr UINT ID_POPUP_CLOSE = 104;

class PopupWindow {
public:
    bool Create(HINSTANCE instance, HWND owner, void* context, PopupCommandHandler command_handler);
    void Destroy();
    void Hide();
    bool IsVisible() const;
    void Show(const CodexTray::AppState& state);
    void Update(const CodexTray::AppState& state);

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void UpdateControls();
    void DrawButton(const DRAWITEMSTRUCT& draw) const;

    HINSTANCE instance_{};
    HWND window_{};
    HWND sign_in_button_{};
    HWND check_button_{};
    HWND refresh_button_{};
    HWND close_button_{};
    void* context_{};
    PopupCommandHandler command_handler_{};
    CodexTray::AppState state_{};
};
