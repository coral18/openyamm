#include "doctest/doctest.h"
#include "game/app/GamepadUiInput.h"
#include "game/app/GameInputSystem.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/ui/GameplayJournalNavigation.h"

#include <vector>

using namespace OpenYAMM::Game;

TEST_CASE("journal menu focus wraps and never reaches an unavailable Atlas entry")
{
    using Target = GameplayJournalPointerTargetType;
    std::vector<Target> available;
    for (const auto &[id, type] : GameplayJournalMainTargets) available.push_back(type);
    CHECK(nextJournalMenuFocus(Target::None, available, false) == Target::MainMapView);
    CHECK(nextJournalMenuFocus(Target::None, available, true) == Target::CloseButton);
    CHECK(nextJournalMenuFocus(Target::CloseButton, available, false) == Target::MainMapView);
    CHECK(nextJournalMenuFocus(Target::MainMapView, available, true) == Target::CloseButton);
    CHECK(nextJournalMenuFocus(Target::MainNotesView, available, false) == Target::InteractiveAtlas);
    std::erase(available, Target::InteractiveAtlas);
    CHECK(nextJournalMenuFocus(Target::MainNotesView, available, false) == Target::CloseButton);
    CHECK(nextJournalMenuFocus(Target::InteractiveAtlas, available, false) == Target::MainMapView);
    CHECK(nextJournalMenuFocus(Target::MainMapView, {}, false) == Target::None);
}

namespace
{
struct VirtualPad
{
    GamepadUiInput input;
    SDL_Joystick *pJoystick = nullptr;
    SDL_JoystickID id = 0;
    int context = 0;
    bool attached = false;
    std::vector<SDL_Event> keys;
    std::vector<SDL_Event> deviceEvents;

    VirtualPad()
    {
        REQUIRE(SDL_InitSubSystem(SDL_INIT_GAMEPAD));
        SDL_VirtualJoystickDesc description = {};
        SDL_INIT_INTERFACE(&description);
        description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        description.naxes = SDL_GAMEPAD_AXIS_COUNT;
        description.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        description.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
        description.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
        description.name = "OpenYAMM isolated controller test";
        id = SDL_AttachVirtualJoystick(&description);
        REQUIRE(id != 0);
        attached = true;
        REQUIRE(SDL_IsGamepad(id));
        pJoystick = SDL_OpenJoystick(id);
        REQUIRE(pJoystick != nullptr);
        // Select this test device before consuming any already connected physical-device notifications.
        SDL_Event added = {};
        added.type = SDL_EVENT_GAMEPAD_ADDED;
        added.gdevice.which = id;
        input.handleEvent(added);
        input.setContext(&context);
        pump();
        frame();
        REQUIRE(input.inputEnabled());
    }

    ~VirtualPad()
    {
        input.shutdown();
        if (pJoystick != nullptr) SDL_CloseJoystick(pJoystick);
        if (attached) SDL_DetachVirtualJoystick(id);
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }

    void pump()
    {
        SDL_UpdateJoysticks();
        SDL_Event event = {};
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP
                || event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) deviceEvents.push_back(event);
            if (const auto key = input.handleEvent(event)) keys.push_back(*key);
        }
    }

    void button(SDL_GamepadButton button, bool down)
    {
        REQUIRE(SDL_SetJoystickVirtualButton(pJoystick, int(button), down));
        pump();
    }

    void axis(SDL_GamepadAxis axis, int16_t value)
    {
        REQUIRE(SDL_SetJoystickVirtualAxis(pJoystick, int(axis), value));
        pump();
    }

    GameplayInputFrame frame(bool blocked = false, int width = 1600, int height = 900)
    {
        GameplayInputFrame result;
        result.screenWidth = width;
        result.screenHeight = height;
        result.pointerX = width / 2.0f;
        result.pointerY = height / 2.0f;
        bool left = false;
        input.update(result, 0.016f, blocked, left);
        result.leftMouseButton.held = left;
        return result;
    }
};
}

TEST_CASE("gamepad UI routes real SDL device buttons to focus accept and back")
{
    VirtualPad pad;
    pad.button(SDL_GAMEPAD_BUTTON_DPAD_UP, true);
    REQUIRE_FALSE(pad.keys.empty());
    CHECK(pad.keys.back().key.scancode == SDL_SCANCODE_TAB);
    pad.button(SDL_GAMEPAD_BUTTON_DPAD_UP, false);
    CHECK(pad.frame().isScancodeHeld(SDL_SCANCODE_LSHIFT)); // Quick reverse tap retains its direction.
    CHECK_FALSE(pad.frame().isScancodeHeld(SDL_SCANCODE_LSHIFT));
    pad.button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    CHECK(pad.keys.back().key.scancode == SDL_SCANCODE_RETURN);
    CHECK(pad.frame().isScancodeHeld(SDL_SCANCODE_RETURN));
    pad.button(SDL_GAMEPAD_BUTTON_SOUTH, false);
    CHECK_FALSE(pad.frame().isScancodeHeld(SDL_SCANCODE_RETURN));
    pad.button(SDL_GAMEPAD_BUTTON_EAST, true);
    CHECK(pad.keys.back().key.scancode == SDL_SCANCODE_ESCAPE);
    pad.button(SDL_GAMEPAD_BUTTON_EAST, false);
    CHECK(pad.frame().isScancodeHeld(SDL_SCANCODE_ESCAPE)); // One-frame Back survives a complete inter-frame tap.
    CHECK_FALSE(pad.frame().isScancodeHeld(SDL_SCANCODE_ESCAPE));
}

TEST_CASE("gamepad UI cursor moves within the game frame and preserves short clicks")
{
    VirtualPad pad;
    pad.axis(SDL_GAMEPAD_AXIS_LEFTX, 2000);
    CHECK_FALSE(pad.frame().gamepadPointerActive);
    pad.axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
    const auto moved = pad.frame();
    CHECK(moved.gamepadPointerActive);
    CHECK(moved.pointerX > 800);
    pad.axis(SDL_GAMEPAD_AXIS_LEFTX, 0);
    pad.axis(SDL_GAMEPAD_AXIS_RIGHTY, 32767);
    CHECK(pad.frame().mouseWheelDelta < 0);
    pad.axis(SDL_GAMEPAD_AXIS_RIGHTY, 0);
    const size_t keysBefore = pad.keys.size();
    pad.button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    pad.button(SDL_GAMEPAD_BUTTON_SOUTH, false);
    CHECK(pad.keys.size() == keysBefore);
    const auto press = pad.frame();
    CHECK(press.leftMouseButton.held);
    CHECK_FALSE(press.pointerCancelled);
    const auto release = pad.frame();
    CHECK_FALSE(release.leftMouseButton.held);
    CHECK_FALSE(release.pointerCancelled);
    pad.axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
    for (int i = 0; i < 200; ++i) pad.frame();
    CHECK(pad.frame().pointerX <= 1599);
    CHECK(pad.frame(false, 800, 450).pointerX <= 799);
}

TEST_CASE("gamepad UI changes of context focus and device cancel drags and require neutral controls")
{
    VirtualPad pad;
    pad.axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
    pad.frame();
    pad.axis(SDL_GAMEPAD_AXIS_LEFTX, 0);
    pad.button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    CHECK(pad.frame().leftMouseButton.held);
    const auto blocked = pad.frame(true);
    CHECK(blocked.pointerCancelled);
    CHECK_FALSE(blocked.leftMouseButton.held);
    CHECK_FALSE(pad.frame().leftMouseButton.held);
    CHECK_FALSE(pad.input.inputEnabled());
    pad.button(SDL_GAMEPAD_BUTTON_SOUTH, false);
    pad.frame();
    CHECK(pad.input.inputEnabled());
    pad.button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    pad.frame();
    int otherContext = 0;
    pad.input.setContext(&otherContext);
    CHECK(pad.frame().pointerCancelled);
    CHECK_FALSE(pad.input.inputEnabled());
    pad.button(SDL_GAMEPAD_BUTTON_SOUTH, false);
    pad.frame();
    pad.button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    CHECK(pad.frame().leftMouseButton.held);
    REQUIRE(SDL_DetachVirtualJoystick(pad.id));
    pad.attached = false;
    pad.pump();
    const auto disconnected = pad.frame();
    CHECK(disconnected.pointerCancelled);
    CHECK_FALSE(disconnected.leftMouseButton.held);
}

TEST_CASE("gamepad opens Map Book as an existing action only outside menu contexts")
{
    VirtualPad pad;
    pad.button(SDL_GAMEPAD_BUTTON_NORTH, true);
    pad.frame();
    CHECK_FALSE(pad.input.mapBookHeld());
    pad.button(SDL_GAMEPAD_BUTTON_NORTH, false);
    pad.input.setContext(nullptr);
    pad.frame();
    pad.button(SDL_GAMEPAD_BUTTON_NORTH, true);
    pad.button(SDL_GAMEPAD_BUTTON_NORTH, false);
    pad.frame();
    CHECK(pad.input.mapBookHeld());
    pad.frame();
    CHECK_FALSE(pad.input.mapBookHeld());
}

TEST_CASE("gamepad shared input preserves event counts and isolates background virtual fixtures")
{
    VirtualPad pad;
    GameInputSystem input;
    GameSettings settings;
    REQUIRE(SDL_GetKeyboardFocus() == nullptr);
    REQUIRE(input.selectVirtualGamepad(pad.id));
    CHECK_FALSE(input.selectVirtualGamepad(0));
    int firstContext = 0, secondContext = 0;
    input.setGamepadUiContext(&firstContext);
    const auto update = [&](SDL_JoystickID fixture)
    {
        input.updateFromEngineInput(1600, 900, 0, settings, false, false, false, false, true, {}, 0.016f, fixture);
    };
    const auto button = [&](SDL_GamepadButton button, bool down)
    {
        pad.deviceEvents.clear();
        pad.button(button, down);
        for (const SDL_Event &event : pad.deviceEvents) input.handleGamepadEvent(event);
    };
    update(pad.id);
    button(SDL_GAMEPAD_BUTTON_DPAD_DOWN, true);
    update(pad.id);
    CHECK(input.frame().scancodePressCount(SDL_SCANCODE_TAB) == 1);
    button(SDL_GAMEPAD_BUTTON_DPAD_DOWN, false);
    update(pad.id);
    CHECK(input.frame().scancodePressCount(SDL_SCANCODE_TAB) == 0);
    button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    button(SDL_GAMEPAD_BUTTON_SOUTH, false);
    input.setGamepadUiContext(&secondContext);
    update(pad.id);
    CHECK(input.frame().scancodePressCount(SDL_SCANCODE_RETURN) == 0);
    button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    button(SDL_GAMEPAD_BUTTON_SOUTH, false);
    update(0); // A real background window cannot receive the test-only virtual-fixture exception implicitly.
    CHECK(input.frame().scancodePressCount(SDL_SCANCODE_RETURN) == 0);
    update(pad.id);
    CHECK(input.frame().scancodePressCount(SDL_SCANCODE_RETURN) == 0);
    button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    button(SDL_GAMEPAD_BUTTON_SOUTH, false);
    SDL_Event motion = {};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    input.handleGamepadEvent(motion);
    update(pad.id);
    CHECK(input.frame().scancodePressCount(SDL_SCANCODE_RETURN) == 1); // Zero-motion notifications retain input.
    button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    button(SDL_GAMEPAD_BUTTON_SOUTH, false);
    motion.motion.xrel = 1;
    input.handleGamepadEvent(motion);
    update(pad.id);
    CHECK(input.frame().scancodePressCount(SDL_SCANCODE_RETURN) == 0);
    input.setGamepadUiContext(nullptr);
    update(pad.id);
    button(SDL_GAMEPAD_BUTTON_NORTH, true);
    button(SDL_GAMEPAD_BUTTON_NORTH, false);
    update(pad.id);
    CHECK(input.frame().action(KeyboardAction::MapBook).pressed);
    update(pad.id);
    CHECK_FALSE(input.frame().action(KeyboardAction::MapBook).held);
    input.setGamepadUiContext(&firstContext);
    update(pad.id);
    button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    button(SDL_GAMEPAD_BUTTON_SOUTH, false);
    VirtualPad replacement;
    REQUIRE(input.selectVirtualGamepad(replacement.id));
    update(replacement.id);
    CHECK(input.frame().scancodePressCount(SDL_SCANCODE_RETURN) == 0);
}
