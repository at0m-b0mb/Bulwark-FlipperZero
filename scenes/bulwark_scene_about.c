#include "../bulwark_i.h"

void bulwark_scene_about_on_enter(void* context) {
    BulwarkApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_text_scroll_element(
        widget,
        0,
        0,
        128,
        64,
        "\e#Bulwark\e#\n"
        "BLE advertising-spam watch\n"
        "for the Flipper's own radio.\n"
        "\n"
        "\e#What it does\e#\n"
        "Parks the radio on the three "
        "Bluetooth advertising channels "
        "(2402, 2426 and 2480 MHz) and on "
        "the three Wi-Fi channel centres "
        "between them, and measures how "
        "much of the time each one is busy.\n"
        "\n"
        "A popup-spam attack fills all "
        "three advertising channels, "
        "equally, from close range, "
        "without stopping. Nothing else in "
        "2.4 GHz does that: those three "
        "frequencies have nothing in "
        "common except Bluetooth.\n"
        "\n"
        "\e#What it cannot do\e#\n"
        "In RF test mode the radio "
        "measures energy and decodes "
        "nothing. Bulwark never sees an "
        "address, a device name, a "
        "payload, or which spam family it "
        "is. It cannot count the devices. "
        "It cannot tell an attack from a "
        "shop full of beacons.\n"
        "\n"
        "So the score stops at 94, never "
        "100, and the breakdown screen "
        "always names the ceiling that "
        "held it there.\n"
        "\n"
        "It never says a place is safe. "
        "It reports what arrived at its "
        "own antenna while it was "
        "listening.\n"
        "\n"
        "\e#Take a baseline\e#\n"
        "One sweep somewhere you trust. "
        "Without it every verdict is "
        "capped at 88, because there is "
        "nothing to say what normal looks "
        "like where you are.\n"
        "\n"
        "\e#While it listens\e#\n"
        "The Flipper's own Bluetooth is "
        "off. It comes back when you "
        "leave the watch screen.\n"
        "\n"
        "\e#Keys\e#\n"
        "Left/Right  wall or trend\n"
        "OK          the breakdown\n"
        "Hold OK     start over\n"
        "\n"
        "\e#Author\e#\n"
        "at0m-b0mb\n"
        "github.com/at0m-b0mb/\n"
        "Bulwark-FlipperZero\n"
        "\n"
        "MIT licence. v1.0\n");

    view_dispatcher_switch_to_view(app->view_dispatcher, BulwarkViewWidget);
}

bool bulwark_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void bulwark_scene_about_on_exit(void* context) {
    BulwarkApp* app = context;
    widget_reset(app->widget);
}
