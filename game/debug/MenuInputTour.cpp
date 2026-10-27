#include "game/debug/MenuInputTour.h"

#include "game/debug/ScreenshotCaptureService.h"
#include "game/ui/IScreen.h"

#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
SDL_Window *tourWindow()
{
    // A scripted review can run while the desktop focus remains on another application.
    int count = 0;
    SDL_Window **ppWindows = SDL_GetWindows(&count);
    SDL_Window *pWindow = count == 1 ? ppWindows[0] : nullptr;
    SDL_free(ppWindows);
    return pWindow;
}
} // namespace

MenuInputTour::~MenuInputTour()
{
    if (SDL_WasInit(SDL_INIT_GAMEPAD) != 0)
    {
        if (m_pVirtualJoystick != nullptr) SDL_CloseJoystick(m_pVirtualJoystick);
        if (m_virtualGamepadId != 0) SDL_DetachVirtualJoystick(m_virtualGamepadId);
    }
    if (m_changedBackgroundGamepadHint)
    {
        if (m_previousBackgroundGamepadHint)
            SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, m_previousBackgroundGamepadHint->c_str());
        else SDL_ResetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS);
    }
}

SDL_JoystickID MenuInputTour::virtualGamepadId() const
{
    return m_virtualGamepadId;
}

std::optional<std::array<float, 2>> MenuInputTour::takeGamepadPointer()
{
    const auto result = m_gamepadPointer;
    m_gamepadPointer.reset();
    return result;
}

void MenuInputTour::update(const std::string &path, GameplayInputFrame &input, IScreen *pScreen,
                           ScreenshotCaptureService &captures, const std::function<void()> &requestPause)
{
    if (m_releaseGamepadButton != SDL_GAMEPAD_BUTTON_INVALID)
    {
        if (!SDL_SetJoystickVirtualButton(m_pVirtualJoystick, int(m_releaseGamepadButton), false))
            throw std::runtime_error(SDL_GetError());
        SDL_UpdateJoysticks();
        m_releaseGamepadButton = SDL_GAMEPAD_BUTTON_INVALID;
    }
    if (path.empty() ||
        (m_loaded && m_index == m_steps.size() && !m_release && !m_exit && m_releaseKey == SDL_SCANCODE_UNKNOWN))
    {
        return;
    }
    if (m_releaseKey != SDL_SCANCODE_UNKNOWN)
    {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_UP;
        event.key.scancode = m_releaseKey;
        event.key.key = SDL_GetKeyFromScancode(m_releaseKey, SDL_KMOD_NONE, false);
        if (SDL_Window *pWindow = tourWindow())
        {
            event.key.windowID = SDL_GetWindowID(pWindow);
        }
        if (pScreen != nullptr)
        {
            pScreen->handleSdlEvent(event);
        }
        else
        {
            SDL_PushEvent(&event);
        }
        m_releaseKey = SDL_SCANCODE_UNKNOWN;
    }
    if (!m_loaded)
    {
        const YAML::Node script = YAML::LoadFile(path);
        m_output = script["output_dir"].as<std::string>();
        if (m_output.is_relative())
        {
            m_output = std::filesystem::path(path).parent_path() / m_output;
        }
        m_exit = script["exit"].as<bool>(true);
        for (const YAML::Node &node : script["steps"])
        {
            if (!node.IsMap() || node.size() != 1)
            {
                throw std::runtime_error("Invalid menu tour step");
            }
            const std::string kind = node.begin()->first.as<std::string>();
            const YAML::Node value = node.begin()->second;
            Step step;
            step.kind = kind;
            if (kind == "gamepad_begin")
            {
                if (!value.as<bool>()) throw std::runtime_error("gamepad_begin must be true");
            }
            else if (kind == "gamepad_axis")
            {
                step.text = value["axis"].as<std::string>();
                step.x = value["value"].as<float>();
                if (!std::isfinite(step.x) || step.x < -1 || step.x > 1)
                    throw std::runtime_error("gamepad_axis value must be in [-1, 1]");
            }
            else if (kind == "click" || kind == "pointer_down" || kind == "pointer_move" || kind == "pointer_up"
                || kind == "gamepad_pointer"
                || kind == "right_down" || kind == "right_up" || kind == "look_rate"
                || kind == "touch_drag_start" || kind == "touch_drag_move" || kind == "touch_drag_end")
            {
                step.x = value[0].as<float>();
                step.y = value[1].as<float>();
            }
            else if (kind == "wait" || kind == "wheel")
            {
                step.seconds = value.as<float>();
            }
            else if (kind == "pause")
            {
                if (!value.as<bool>())
                    throw std::runtime_error("Menu tour pause must be true");
            }
            else if (kind == "key" || kind == "key_down" || kind == "key_up" || kind == "text" || kind == "capture" ||
                     kind == "expect" || kind == "action" || kind == "action_down" || kind == "action_up"
                     || kind == "gamepad_button" || kind == "gamepad_button_down" || kind == "gamepad_button_up")
            {
                step.text = value.as<std::string>();
            }
            else
            {
                throw std::runtime_error("Unknown menu tour step: " + kind);
            }
            m_steps.push_back(std::move(step));
        }
        m_loaded = true;
        m_nextTicks = SDL_GetTicks() + 2000;
    }
    const float scale = std::min(input.screenWidth / 1600.0f, input.screenHeight / 900.0f);
    if (m_virtualGamepadId == 0)
    {
        input.pointerX = (input.screenWidth - 1600 * scale) / 2 + m_x * scale;
        input.pointerY = (input.screenHeight - 900 * scale) / 2 + m_y * scale;
    }
    if (m_touchDragActive)
    {
        input.mobileTouchDragActive = true;
        input.mobileTouchDragStartX = (input.screenWidth - 1600 * scale) / 2 + m_touchDragStartX * scale;
        input.mobileTouchDragStartY = (input.screenHeight - 900 * scale) / 2 + m_touchDragStartY * scale;
    }
    if (m_virtualGamepadId == 0)
    {
        input.leftMouseButton = {m_release || m_pointerHeld, false, false};
        input.rightMouseButton = {m_rightPointerHeld, false, false};
    }
    for (size_t key = 0; key < m_heldKeys.size(); ++key)
    {
        input.keyboardHeld[key] = input.keyboardHeld[key] || m_heldKeys[key];
    }
    for (size_t actionIndex = 0; actionIndex < m_heldActions.size(); ++actionIndex)
    {
        input.actions[actionIndex].held = input.actions[actionIndex].held || m_heldActions[actionIndex];
    }
    const uint64_t inputTicks = SDL_GetTicksNS();
    const float inputSeconds = m_lastInputTicks == 0 ? 0.0f : float(inputTicks - m_lastInputTicks) * 1.0e-9f;
    m_lastInputTicks = inputTicks;
    input.relativeMouseX += m_lookRateX * inputSeconds;
    input.relativeMouseY += m_lookRateY * inputSeconds;
    if (SDL_GetTicks() < m_nextTicks)
    {
        return;
    }
    if (m_release)
    {
        input.leftMouseButton = {false, false, true};
        m_release = false;
        m_nextTicks = SDL_GetTicks() + 250;
        return;
    }
    if (m_index == m_steps.size())
    {
        if (m_exit && !captures.hasPendingCaptures())
        {
            m_exit = false;
            SDL_Event event{};
            event.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&event);
        }
        return;
    }
    const Step &step = m_steps[m_index++];
    std::cout << "Menu tour: " << m_index << ' ' << step.kind << ' ' << step.text << std::endl;
    m_nextTicks = SDL_GetTicks() + 350;
    if (step.kind == "gamepad_begin")
    {
        if (m_virtualGamepadId != 0) throw std::runtime_error("Gamepad fixture already exists");
        if (const char *pHint = SDL_GetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS))
            m_previousBackgroundGamepadHint = pHint;
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        m_changedBackgroundGamepadHint = true;
        SDL_VirtualJoystickDesc description = {};
        SDL_INIT_INTERFACE(&description);
        description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        description.naxes = SDL_GAMEPAD_AXIS_COUNT;
        description.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        description.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
        description.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
        description.name = "OpenYAMM native menu tour";
        m_virtualGamepadId = SDL_AttachVirtualJoystick(&description);
        if (m_virtualGamepadId == 0) throw std::runtime_error(SDL_GetError());
        m_pVirtualJoystick = SDL_OpenJoystick(m_virtualGamepadId);
        if (m_pVirtualJoystick == nullptr) throw std::runtime_error(SDL_GetError());
    }
    else if (step.kind.starts_with("gamepad_"))
    {
        if (m_pVirtualJoystick == nullptr) throw std::runtime_error("gamepad_begin is required first");
        if (step.kind == "gamepad_pointer")
        {
            m_gamepadPointer = {{((input.screenWidth - 1600 * scale) / 2 + step.x * scale)
                    / std::max(1, input.screenWidth - 1),
                ((input.screenHeight - 900 * scale) / 2 + step.y * scale) / std::max(1, input.screenHeight - 1)}};
        }
        else if (step.kind == "gamepad_axis")
        {
            const SDL_GamepadAxis axis = SDL_GetGamepadAxisFromString(step.text.c_str());
            if (axis == SDL_GAMEPAD_AXIS_INVALID
                || !SDL_SetJoystickVirtualAxis(m_pVirtualJoystick, int(axis), int16_t(step.x * 32767)))
                throw std::runtime_error("Invalid gamepad axis: " + step.text);
        }
        else
        {
            const SDL_GamepadButton button = SDL_GetGamepadButtonFromString(step.text.c_str());
            if (button == SDL_GAMEPAD_BUTTON_INVALID || !SDL_SetJoystickVirtualButton(m_pVirtualJoystick,
                int(button), step.kind != "gamepad_button_up"))
                throw std::runtime_error("Invalid gamepad button: " + step.text);
            if (step.kind == "gamepad_button") m_releaseGamepadButton = button;
        }
        SDL_UpdateJoysticks();
    }
    else if (step.kind == "pause")
    {
        if (pScreen != nullptr || !requestPause)
            throw std::runtime_error("Menu tour pause requires active gameplay");
        requestPause();
    }
    else if (step.kind == "wait")
    {
        m_nextTicks = SDL_GetTicks() + uint64_t(std::max(0.0f, step.seconds) * 1000);
    }
    else if (step.kind == "look_rate")
    {
        m_lookRateX = step.x;
        m_lookRateY = step.y;
    }
    else if (step.kind == "click" || step.kind == "pointer_down" || step.kind == "pointer_move" ||
             step.kind == "pointer_up")
    {
        m_x = step.x;
        m_y = step.y;
        input.pointerX = (input.screenWidth - 1600 * scale) / 2 + m_x * scale;
        input.pointerY = (input.screenHeight - 900 * scale) / 2 + m_y * scale;
        if (step.kind != "pointer_move")
        {
            const bool down = step.kind != "pointer_up";
            input.leftMouseButton = {down, down, !down};
            m_pointerHeld = step.kind == "pointer_down";
            m_release = step.kind == "click";
            m_nextTicks = SDL_GetTicks() + 90;
        }
    }
    else if (step.kind == "touch_drag_start" || step.kind == "touch_drag_move" || step.kind == "touch_drag_end")
    {
        // Replays the shared UI frames after Android has recognized a drag, without desktop input injection.
        const bool start = step.kind == "touch_drag_start";
        if (start == m_touchDragActive)
            throw std::runtime_error("Menu tour touch drag has no matching start/end");
        const bool wasHeld = m_pointerHeld;
        m_x = step.x;
        m_y = step.y;
        if (start)
        {
            m_touchDragStartX = m_x;
            m_touchDragStartY = m_y;
        }
        m_touchDragActive = step.kind != "touch_drag_end";
        m_pointerHeld = m_touchDragActive;
        input.pointerX = (input.screenWidth - 1600 * scale) / 2 + m_x * scale;
        input.pointerY = (input.screenHeight - 900 * scale) / 2 + m_y * scale;
        input.mobileTouchDragStartX = (input.screenWidth - 1600 * scale) / 2 + m_touchDragStartX * scale;
        input.mobileTouchDragStartY = (input.screenHeight - 900 * scale) / 2 + m_touchDragStartY * scale;
        input.mobileTouchDragStarted = start;
        input.mobileTouchDragActive = m_touchDragActive;
        input.mobileTouchDragReleased = !m_touchDragActive;
        input.leftMouseButton = {m_pointerHeld, m_pointerHeld && !wasHeld, !m_pointerHeld};
    }
    else if (step.kind == "right_down" || step.kind == "right_up")
    {
        m_x = step.x;
        m_y = step.y;
        input.pointerX = (input.screenWidth - 1600 * scale) / 2 + m_x * scale;
        input.pointerY = (input.screenHeight - 900 * scale) / 2 + m_y * scale;
        m_rightPointerHeld = step.kind == "right_down";
        input.rightMouseButton = {m_rightPointerHeld, m_rightPointerHeld, !m_rightPointerHeld};
    }
    else if (step.kind == "action" || step.kind == "action_down" || step.kind == "action_up")
    {
        const auto found = std::find_if(keyboardBindingDefinitions().begin(), keyboardBindingDefinitions().end(),
            [&](const KeyboardBindingDefinition &definition) { return definition.iniKey == step.text; });
        if (found == keyboardBindingDefinitions().end())
        {
            throw std::runtime_error("Unknown gameplay tour action: " + step.text);
        }
        const size_t actionIndex = keyboardActionIndex(found->action);
        const bool down = step.kind != "action_up";
        if (step.kind != "action")
        {
            m_heldActions[actionIndex] = down;
        }
        input.actions[actionIndex] = {down, down, !down};
    }
    else if (step.kind == "wheel")
    {
        input.mouseWheelDelta = step.seconds;
    }
    else if (step.kind == "capture")
    {
        captures.requestCapture(m_output / (step.text + ".png"),
                                [](bool success, const std::string &message)
                                {
                                    if (!success)
                                    {
                                        throw std::runtime_error(message);
                                    }
                                    std::cout << "Menu tour: " << message << '\n';
                                });
    }
    else if (step.kind == "expect")
    {
        SDL_Window *pWindow = tourWindow();
        const bool matched = (step.text == "gameplay" && pScreen == nullptr) ||
                             (step.text == "pause" && pScreen != nullptr && pScreen->mode() == AppMode::PauseMenu) ||
                             (step.text == "text_input" && pWindow != nullptr && SDL_TextInputActive(pWindow)) ||
                             (step.text == "no_text_input" && pWindow != nullptr && !SDL_TextInputActive(pWindow)) ||
                             (step.text == "menu_cursor" && pWindow != nullptr &&
                              !SDL_GetWindowRelativeMouseMode(pWindow) && SDL_CursorVisible());
        if (!matched)
        {
            throw std::runtime_error("Menu tour expectation failed: " + step.text);
        }
    }
    else
    {
        SDL_Event event{};
        if (step.kind == "text")
        {
            SDL_Window *pWindow = tourWindow();
            if (pWindow == nullptr || !SDL_TextInputActive(pWindow))
            {
                throw std::runtime_error("Menu tour text entry requires active SDL text input");
            }
            event.type = SDL_EVENT_TEXT_INPUT;
            event.text.windowID = SDL_GetWindowID(pWindow);
            event.text.text = step.text.c_str();
            SDL_PushEvent(&event);
            return;
        }
        else
        {
            const SDL_Scancode code = SDL_GetScancodeFromName(step.text.c_str());
            if (code == SDL_SCANCODE_UNKNOWN)
            {
                throw std::runtime_error("Unknown menu tour key: " + step.text);
            }
            const bool down = step.kind != "key_up";
            input.keyboardHeld[code] = down;
            // Gameplay receives the queued SDL event next frame; counting it here would deliver the tap twice.
            input.keyboardPressCounts[code] = down && pScreen != nullptr ? 1 : 0;
            m_heldKeys[code] = step.kind == "key_down";
            if (step.kind == "key")
            {
                m_releaseKey = code;
            }
            event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            event.key.scancode = code;
            event.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false);
            if (SDL_Window *pWindow = tourWindow())
            {
                event.key.windowID = SDL_GetWindowID(pWindow);
            }
        }
        if (pScreen != nullptr)
        {
            pScreen->handleSdlEvent(event);
        }
        else
        {
            // Gameplay/debug-console keys belong to the normal SDL event path when there is no UI screen.
            SDL_PushEvent(&event);
        }
    }
}
} // namespace OpenYAMM::Game
