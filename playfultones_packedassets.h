/** BEGIN_JUCE_MODULE_DECLARATION
ID:               playfultones_packedassets
vendor:           Playful Tones
version:          0.1.0
name:             Packed Assets
description:      Encrypted single-copy asset packaging for JUCE plugins.
website:          https://playfultones.com
license:          MIT
dependencies:     juce_core, juce_graphics, juce_audio_formats, juce_gui_basics
windowsLibs:      bcrypt
mingwLibs:        bcrypt
END_JUCE_MODULE_DECLARATION
*/
#pragma once
#define PLAYFULTONES_PACKEDASSETS_H_INCLUDED

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "src/PackedAssetSource.h"
#include "src/TypedAssets.h"
#include "src/Source.h"
