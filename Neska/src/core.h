#pragma once

enum class MirrorMode {
    HORIZONTAL,
    VERTICAL,
    FOUR_SCREEN,
    SINGLE_SCREEN
};

enum class PPUStatusFlag {
    VBlank,
    Sprite0Hit,
    SpriteOverflow,
    NMI
};