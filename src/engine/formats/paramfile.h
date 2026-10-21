#pragma once

#include <string>

#include "convert/param.h"
#include "convert/screen.h"

namespace tuxscr {

// The settings as ConvImgCpc keeps them in a file (.xml): what C#'s
// serializer writes of its Param. TuxSCR adds the palette interrupts at the
// end, which ConvImgCpc passes over when it reads the file.
std::string paramXml(const Param& param, const Screen& screen);

// Reads such a file: false if it is not one. What the file does not say is
// left as it is.
bool readParamXml(const std::string& xml, Param& param, Screen& screen);

}  // namespace tuxscr
