#include "game/app/GamepadUiInput.h"
#include "game/gameplay/GameplayInputFrame.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace OpenYAMM::Game
{
namespace
{
float stickAxis(int16_t value)
{
    const float normalized = std::clamp(float(value) / 32767.0f, -1.0f, 1.0f);
    return std::abs(normalized) <= 0.2f ? 0.0f
        : std::copysign((std::abs(normalized) - 0.2f) / 0.8f, normalized);
}

int focusDirection(uint8_t button)
{
    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return -1;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return 1;
    default:
        return 0;
    }
}
}

GamepadUiInput::~GamepadUiInput()
{
    shutdown();
}

void GamepadUiInput::cancelInteraction()
{
    m_pointerCancelled = m_pointerCancelled || m_primaryAsPointer || m_pointerPressPending || m_pointerHeldLastFrame;
    m_pointerHeldLastFrame = false;
    m_primaryAsPointer = m_pointerPressPending = false;
    m_focusDirection = 0;
    m_pendingFocusDirection = 0;
    m_backPending = false;
    m_mapBookPending = m_mapBookHeld = false;
    m_waitForNeutral = true;
}

void GamepadUiInput::shutdown()
{
    cancelInteraction();
    if (m_pGamepad != nullptr && SDL_WasInit(SDL_INIT_GAMEPAD) != 0)
        SDL_CloseGamepad(m_pGamepad);
    m_pGamepad = nullptr;
    m_id = 0;
    m_buttons.fill(false);
    m_pointerMode = false;
    m_enumerated = false;
}

void GamepadUiInput::open(SDL_JoystickID id)
{
    if (m_pGamepad != nullptr || !SDL_IsGamepad(id)) return;
    m_pGamepad = SDL_OpenGamepad(id);
    if (m_pGamepad != nullptr)
    {
        m_id = id;
        if (const char *pName = SDL_GetGamepadName(m_pGamepad))
            std::cout << "Gamepad: " << pName << " (SDL ID " << m_id << ")\n";
        cancelInteraction();
    }
}

bool GamepadUiInput::setContext(const void *pContext)
{
    if (m_pContext == pContext) return false;
    cancelInteraction();
    m_pContext = pContext;
    return true;
}

bool GamepadUiInput::neutral() const
{
    if (std::any_of(m_buttons.begin(), m_buttons.end(), [](bool held) { return held; })) return false;
    if (m_pGamepad == nullptr) return true;
    for (SDL_GamepadAxis axis : {SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_AXIS_RIGHTY})
        if (stickAxis(SDL_GetGamepadAxis(m_pGamepad, axis)) != 0) return false;
    return true;
}

SDL_Event GamepadUiInput::keyEvent(uint32_t type, SDL_Scancode scancode) const
{
    SDL_Event event = {};
    event.type = type;
    event.key.scancode = scancode;
    event.key.key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
    event.key.down = type == SDL_EVENT_KEY_DOWN;
    if (scancode == SDL_SCANCODE_TAB && m_focusDirection < 0) event.key.mod = SDL_KMOD_LSHIFT;
    if (SDL_Window *pWindow = SDL_GetKeyboardFocus()) event.key.windowID = SDL_GetWindowID(pWindow);
    return event;
}

std::optional<SDL_Event> GamepadUiInput::handleEvent(const SDL_Event &event)
{
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) open(event.gdevice.which);
    if (event.type == SDL_EVENT_GAMEPAD_REMOVED && event.gdevice.which == m_id) shutdown();
    if ((event.type == SDL_EVENT_MOUSE_MOTION && event.motion.which != SDL_TOUCH_MOUSEID
            && (event.motion.xrel != 0 || event.motion.yrel != 0)) || event.type == SDL_EVENT_FINGER_DOWN)
    {
        if (m_pointerMode) cancelInteraction();
        m_pointerMode = false;
    }
    if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event.gaxis.which == m_id
        && event.gaxis.axis <= SDL_GAMEPAD_AXIS_LEFTY && stickAxis(event.gaxis.value) != 0
        && m_pContext != nullptr && !m_blocked && !m_waitForNeutral)
        m_pointerMode = true;
    if ((event.type != SDL_EVENT_GAMEPAD_BUTTON_DOWN && event.type != SDL_EVENT_GAMEPAD_BUTTON_UP)
        || event.gbutton.which != m_id || m_id == 0 || event.gbutton.button >= m_buttons.size())
        return std::nullopt;
    const uint8_t button = event.gbutton.button;
    const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    m_buttons[button] = down;
    if (m_blocked || m_waitForNeutral) return std::nullopt;
    if (m_pContext == nullptr)
    {
        if (down && button == SDL_GAMEPAD_BUTTON_NORTH) m_mapBookPending = true;
        return std::nullopt;
    }
    const int direction = focusDirection(button);
    if (direction != 0)
    {
        m_pointerMode = false;
        if (down)
        {
            if (m_primaryAsPointer || m_pointerHeldLastFrame)
            {
                cancelInteraction();
                return std::nullopt;
            }
            m_focusDirection = direction;
            m_pendingFocusDirection = direction;
            m_nextFocusRepeat = SDL_GetTicks() + 350;
        }
        else m_focusDirection = 0;
        return keyEvent(down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP, SDL_SCANCODE_TAB);
    }
    if (button == SDL_GAMEPAD_BUTTON_EAST)
    {
        m_backPending = m_backPending || down;
        return keyEvent(down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP, SDL_SCANCODE_ESCAPE);
    }
    if (button == SDL_GAMEPAD_BUTTON_SOUTH)
    {
        if (down)
        {
            m_primaryAsPointer = m_pointerMode;
            m_pointerPressPending = m_primaryAsPointer;
        }
        if (m_primaryAsPointer)
        {
            if (!down) m_primaryAsPointer = false;
            return std::nullopt;
        }
        return keyEvent(down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP, SDL_SCANCODE_RETURN);
    }
    return std::nullopt;
}

void GamepadUiInput::update(GameplayInputFrame &frame, float deltaSeconds, bool blocked, bool &leftMouseHeld)
{
    blocked = blocked || frame.screenWidth <= 0 || frame.screenHeight <= 0;
    if (!m_enumerated && SDL_WasInit(SDL_INIT_GAMEPAD) != 0)
    {
        m_enumerated = true;
        int count = 0;
        SDL_JoystickID *pIds = SDL_GetGamepads(&count);
        for (int i = 0; i < count && m_pGamepad == nullptr; ++i) open(pIds[i]);
        SDL_free(pIds);
    }
    if (m_pGamepad != nullptr)
        for (size_t i = 0; i < m_buttons.size(); ++i)
            m_buttons[i] = SDL_GetGamepadButton(m_pGamepad, SDL_GamepadButton(i));
    if (blocked || m_blocked != blocked) cancelInteraction();
    m_blocked = blocked;
    if (!blocked && neutral()) m_waitForNeutral = false;
    frame.pointerCancelled = m_pointerCancelled;
    m_pointerCancelled = false;
    if (blocked || m_waitForNeutral || m_pGamepad == nullptr) return;
    m_mapBookHeld = m_pContext == nullptr && (m_mapBookPending || m_buttons[SDL_GAMEPAD_BUTTON_NORTH]);
    m_mapBookPending = false;
    if (m_pContext == nullptr) return;
    frame.keyboardHeld[SDL_SCANCODE_ESCAPE] = frame.keyboardHeld[SDL_SCANCODE_ESCAPE]
        || m_buttons[SDL_GAMEPAD_BUTTON_EAST] || m_backPending;
    m_backPending = false;
    frame.keyboardHeld[SDL_SCANCODE_RETURN] = frame.keyboardHeld[SDL_SCANCODE_RETURN]
        || (m_buttons[SDL_GAMEPAD_BUTTON_SOUTH] && !m_primaryAsPointer);
    if (m_pendingFocusDirection < 0) frame.keyboardHeld[SDL_SCANCODE_LSHIFT] = true;
    m_pendingFocusDirection = 0;
    if (m_focusDirection != 0)
    {
        frame.keyboardHeld[SDL_SCANCODE_TAB] = true;
        if (m_focusDirection < 0) frame.keyboardHeld[SDL_SCANCODE_LSHIFT] = true;
        if (SDL_GetTicks() >= m_nextFocusRepeat)
        {
            ++frame.keyboardPressCounts[SDL_SCANCODE_TAB];
            m_nextFocusRepeat = SDL_GetTicks() + 110;
        }
    }
    const float seconds = std::isfinite(deltaSeconds) ? std::clamp(deltaSeconds, 0.0f, 0.05f) : 0.0f;
    if (m_pointerMode && frame.screenWidth > 0 && frame.screenHeight > 0)
    {
        const float x = stickAxis(SDL_GetGamepadAxis(m_pGamepad, SDL_GAMEPAD_AXIS_LEFTX));
        const float y = stickAxis(SDL_GetGamepadAxis(m_pGamepad, SDL_GAMEPAD_AXIS_LEFTY));
        m_pointerU = std::clamp(m_pointerU + x * seconds * 0.7f * frame.screenHeight / frame.screenWidth, 0.0f, 1.0f);
        m_pointerV = std::clamp(m_pointerV + y * seconds * 0.7f, 0.0f, 1.0f);
        frame.pointerX = m_pointerU * std::max(0, frame.screenWidth - 1);
        frame.pointerY = m_pointerV * std::max(0, frame.screenHeight - 1);
        frame.gamepadPointerActive = true;
        frame.mouseWheelDelta -= stickAxis(SDL_GetGamepadAxis(m_pGamepad, SDL_GAMEPAD_AXIS_RIGHTY)) * seconds * 12;
    }
    else if (frame.screenWidth > 0 && frame.screenHeight > 0)
    {
        m_pointerU = std::clamp(frame.pointerX / std::max(1, frame.screenWidth - 1), 0.0f, 1.0f);
        m_pointerV = std::clamp(frame.pointerY / std::max(1, frame.screenHeight - 1), 0.0f, 1.0f);
    }
    m_pointerHeldLastFrame = m_primaryAsPointer || m_pointerPressPending;
    leftMouseHeld = leftMouseHeld || m_pointerHeldLastFrame;
    m_pointerPressPending = false;
}

bool GamepadUiInput::mapBookHeld() const
{
    return m_mapBookHeld;
}

bool GamepadUiInput::inputEnabled() const
{
    return !m_blocked && !m_waitForNeutral && m_pGamepad != nullptr;
}

SDL_JoystickID GamepadUiInput::deviceId() const
{
    return m_id;
}

bool GamepadUiInput::selectVirtualDevice(SDL_JoystickID id)
{
    if (id == 0 || !SDL_IsJoystickVirtual(id) || !SDL_IsGamepad(id)) return false;
    if (m_id != id)
    {
        shutdown();
        open(id);
        m_enumerated = true;
    }
    return m_id == id;
}

bool GamepadUiInput::positionVirtualPointer(SDL_JoystickID id, float u, float v)
{
    if (id == 0 || id != m_id || !SDL_IsJoystickVirtual(id) || !std::isfinite(u) || !std::isfinite(v)
        || u < 0 || u > 1 || v < 0 || v > 1) return false;
    m_pointerU = u;
    m_pointerV = v;
    m_pointerMode = true;
    return true;
}
}
