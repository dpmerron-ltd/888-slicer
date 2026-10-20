#include "libslic3r/I18N.hpp"

// The full application owns this callback in utils.cpp. The standalone parser
// tests use untranslated English and do not link the geometry library.
Slic3r::I18N::translate_fn_type Slic3r::I18N::translate_fn = nullptr;
