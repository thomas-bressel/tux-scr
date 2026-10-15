#pragma once

class ConverterWindow;

// Gives the converter's window split rasters, the Plus's table of rasters and the maker of palettes.
// Called once the window is made, by those who make it.
void installSplitTools(ConverterWindow* window);
