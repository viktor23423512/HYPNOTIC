#pragma once
#include <cmath>
// Numeric/precision budget, independent of fixed mesh and particle capacities.
// Zoom follows original MilkDrop CPU powf/reciprocal semantics; accept signed
// finite nonzero input and validate every computed UV before GPU submission.
constexpr double presetCoordinateLimit=10000.0,presetMinimumScale=0.0001;
inline bool validPresetScalar(double x){return std::isfinite(x) && std::fabs(x)<=presetCoordinateLimit;}
inline bool validPresetZoom(double x){return std::isfinite(x) && x!=0;}
inline bool validPresetStretch(double x){return validPresetScalar(x) && std::fabs(x)>=presetMinimumScale;}
inline float presetSurfaceDecay(double x){return (float)(x<0?0:x>1?1:x);}
