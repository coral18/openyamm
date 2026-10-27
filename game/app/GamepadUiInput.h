#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <optional>

namespace OpenYAMM::Game
{
struct GameplayInputFrame;

// Owns one SDL device and routes menu input into the game's frame, never the system pointer/keyboard.
class GamepadUiInput
{
public:
    GamepadUiInput() = default;
    ~GamepadUiInput();
    GamepadUiInput(const GamepadUiInput &) = delete;
    GamepadUiInput &operator=(const GamepadUiInput &) = delete;

    bool setContext(const void *pContext);
    std::optional<SDL_Event> handleEvent(const SDL_Event &event);
    void update(GameplayInputFrame &frame, float deltaSeconds, bool blocked, bool &leftMouseHeld);
    bool mapBookHeld() const;
    bool inputEnabled() const;
    SDL_JoystickID deviceId() const;
    bool selectVirtualDevice(SDL_JoystickID id);
    bool positionVirtualPointer(SDL_JoystickID id, float u, float v);
    void shutdown();

private:
    void open(SDL_JoystickID id);
    void cancelInteraction();
    SDL_Event keyEvent(uint32_t type, SDL_Scancode scancode) const;
    bool neutral() const;

    SDL_Gamepad *m_pGamepad = nullptr;
    SDL_JoystickID m_id = 0;
    const void *m_pContext = nullptr;
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> m_buttons = {};
    bool m_enumerated = false;
    bool m_blocked = true;
    bool m_waitForNeutral = true;
    bool m_pointerMode = false;
    bool m_primaryAsPointer = false;
    bool m_pointerPressPending = false;
    bool m_pointerHeldLastFrame = false;
    bool m_pointerCancelled = false;
    bool m_mapBookPending = false;
    bool m_mapBookHeld = false;
    float m_pointerU = 0.5f, m_pointerV = 0.5f;
    int m_focusDirection = 0;
    int m_pendingFocusDirection = 0;
    bool m_backPending = false;
    uint64_t m_nextFocusRepeat = 0;
};
}
