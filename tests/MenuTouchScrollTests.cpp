#include "game/ui/MenuTouchScroll.h"
#include <doctest/doctest.h>

using namespace OpenYAMM::Game;

TEST_CASE("menu touch scrolling retains its pane and reverses immediately at a boundary")
{
    MenuTouchScroll gesture;
    float left = 100, right = 200;
    GameplayInputFrame input;
    input.mobileTouchDragStarted = true;
    input.mobileTouchDragActive = true;
    input.mobileTouchDragStartY = 400;
    input.pointerY = 340;
    gesture.beginFrame(input);
    CHECK(gesture.update(input, true, 2, 300, left));
    CHECK(left == 130);
    CHECK_FALSE(gesture.update(input, true, 2, 300, right));
    CHECK(right == 200);

    input.mobileTouchDragStarted = false;
    input.pointerY = 500;
    gesture.beginFrame(input);
    CHECK_FALSE(gesture.update(input, true, 2, 300, right));
    CHECK(gesture.update(input, false, 2, 300, left)); // Finger has left the original pane.
    CHECK(left == 50);
    input.pointerY = 900;
    gesture.beginFrame(input);
    CHECK(gesture.update(input, false, 2, 300, left));
    CHECK(left == 0);
    input.pointerY = 890;
    gesture.beginFrame(input);
    CHECK(gesture.update(input, false, 2, 300, left));
    CHECK(left == 5);

    input.mobileTouchDragActive = false;
    input.mobileTouchDragReleased = true;
    input.pointerY = 850;
    gesture.beginFrame(input);
    CHECK(gesture.update(input, false, 2, 300, left));
    CHECK(left == 25);
    // A new gesture can choose another pane without an intervening idle input frame.
    input.mobileTouchDragStarted = true;
    input.mobileTouchDragActive = true;
    input.mobileTouchDragReleased = false;
    input.mobileTouchDragStartY = 300;
    input.pointerY = 250;
    gesture.beginFrame(input);
    CHECK_FALSE(gesture.update(input, false, 1, 300, left));
    CHECK(gesture.update(input, true, 1, 300, right));
    CHECK(right == 250);
    input.pointerY = 0;
    input.mobileTouchDragStarted = false;
    gesture.beginFrame(input);
    CHECK(gesture.update(input, false, 1, 300, right));
    CHECK(right == 300);
    input.pointerY = 10;
    gesture.beginFrame(input);
    CHECK(gesture.update(input, false, 1, 300, right));
    CHECK(right == 290);
}

TEST_CASE("menu touch scrolling handles a complete between-frame drag at different UI scales")
{
    for (const float scale : {0.5f, 1.0f, 2.5f})
    {
        MenuTouchScroll gesture;
        float scroll = 20;
        GameplayInputFrame input;
        input.mobileTouchDragStarted = true;
        input.mobileTouchDragActive = true;
        input.mobileTouchDragStartY = 100 * scale;
        input.pointerY = input.mobileTouchDragStartY;
        gesture.beginFrame(input);
        CHECK(gesture.update(input, true, scale, 500, scroll));
        CHECK(scroll == 20);
        input.mobileTouchDragStarted = false;
        input.mobileTouchDragActive = false;
        input.mobileTouchDragReleased = true;
        input.pointerY = 40 * scale;
        gesture.beginFrame(input);
        CHECK(gesture.update(input, false, scale, 500, scroll));
        CHECK(scroll == 80);
        input.mobileTouchDragReleased = false;
        input.pointerY = 0;
        gesture.beginFrame(input);
        CHECK_FALSE(gesture.update(input, true, scale, 500, scroll));
        CHECK(scroll == 80);
    }
}

TEST_CASE("menu touch scrolling leaves taps mouse input and unrelated gestures unchanged")
{
    MenuTouchScroll gesture;
    float scroll = 100;
    GameplayInputFrame input;
    input.leftMouseButton = {true, true, false};
    input.pointerY = 0;
    input.mouseWheelDelta = 5;
    gesture.beginFrame(input);
    CHECK_FALSE(gesture.update(input, true, 1, 300, scroll));
    CHECK(scroll == 100);
    input.mobileTouchDragStarted = true;
    input.mobileTouchDragActive = true;
    input.mobileTouchDragStartY = 200;
    gesture.beginFrame(input);
    CHECK_FALSE(gesture.update(input, false, 1, 300, scroll)); // Started outside, or on the scrollbar.
    input.mobileTouchDragStarted = false;
    gesture.beginFrame(input);
    CHECK_FALSE(gesture.update(input, true, 1, 300, scroll)); // Entering a pane never captures it.
    CHECK(scroll == 100);
    input.mobileTouchDragStarted = true;
    gesture.beginFrame(input);
    CHECK_FALSE(gesture.update(input, true, 1, 0, scroll));
    CHECK_FALSE(gesture.update(input, true, 0, 300, scroll));
    CHECK(scroll == 100);
    CHECK(gesture.update(input, true, 1, 300, scroll));
    gesture.reset(); // Closing a modal/screen cancels its previous capture.
    input.mobileTouchDragStarted = false;
    gesture.beginFrame(input);
    CHECK_FALSE(gesture.update(input, true, 1, 300, scroll));
}
