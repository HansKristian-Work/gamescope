#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <vector>

#include <glm/vec2.hpp> // glm::vec2
#include <glm/vec3.hpp> // glm::vec3
#include <glm/mat3x3.hpp> // glm::mat3

// Color utils
inline int quantize( float fVal, float fMaxVal )
{
    return std::max( 0.f, std::min( fMaxVal, roundf( fVal * fMaxVal ) ) );
}

inline uint16_t drm_quantize_lut_value( float flValue )
{
    return (uint16_t)quantize( flValue, (float)UINT16_MAX );
}

inline float clamp01( float val )
{
	return std::max( 0.f, std::min( 1.f, val ) );
}

inline float clamp( float val, float lo, float hi )
{
	return std::max( lo, std::min( hi, val ) );
}

inline float cfit( float x, float i1, float i2, float f1, float f2 )
{
    return f1+(f2-f1)*clamp01( (x-i1)/(i2-i1) );
}

inline float srgb_to_linear( float fVal )
{
    return ( fVal < 0.04045f ) ? fVal / 12.92f : std::pow( ( fVal + 0.055f) / 1.055f, 2.4f );
}

inline float linear_to_srgb( float fVal )
{
    return ( fVal < 0.0031308f ) ? fVal * 12.92f : std::pow( fVal, 1.0f / 2.4f ) * 1.055f - 0.055f;
}

inline float pq_to_nits( float pq )
{
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;

    const float oo_m1 = 1.0f / 0.1593017578125f;
    const float oo_m2 = 1.0f / 78.84375f;

    float num = std::max(std::pow(pq, oo_m2) - c1, 0.0f);
    float den = c2 - c3 * std::pow(pq, oo_m2);

    return 10000.0f * std::pow(num / den, oo_m1);
}

inline float nits_to_pq( float nits )
{
    float y = clamp(nits / 10000.0f, 0.0f, 1.0f);
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    const float m1 = 0.1593017578125f;
    const float m2 = 78.84375f;
    float num = c1 + c2 * std::pow(y, m1);
    float den = 1.0 + c3 * std::pow(y, m1);
    float n = std::pow(num / den, m2);
    return n;
}

inline float flerp( float a, float b, float t )
{
    return a + t * (b - a);
}

inline float safe_pow(float x, float y)
{
	// Avoids pow(x, 1.0f) != x.
	if (y == 1.0f)
		return x;

	return std::pow(std::max(x, 0.0f), y);
}

inline float positive_mod( float flX, float flPeriod )
{
	float flVal = fmodf( flX, flPeriod );
	return ( flVal < 0 ) ? flVal + flPeriod : fabsf( flVal ); // fabs fixes -0
}

// Colorimetry functions related to color space conversions
struct primaries_t
{
	bool operator == (const primaries_t&) const = default;
	bool operator != (const primaries_t&) const = default;

	glm::vec2 r;
	glm::vec2 g;
	glm::vec2 b;
};

enum EOTF
{
	EOTF_Gamma22 = 0,
	EOTF_PQ = 1,

	EOTF_Count = 2,
};

struct displaycolorimetry_t
{
	bool operator == (const displaycolorimetry_t&) const = default;
	bool operator != (const displaycolorimetry_t&) const = default;
	primaries_t primaries;
	glm::vec2 white;
};

struct nightmode_t
{
	bool operator == (const nightmode_t&) const = default;
	bool operator != (const nightmode_t&) const = default;

    float amount; // [0 = disabled, 1.f = on]
    float hue; // [0,1]
    float saturation; // [0,1]
};

struct colormapping_t
{
	bool operator == (const colormapping_t&) const = default;
	bool operator != (const colormapping_t&) const = default;

    float blendEnableMinSat;
    float blendEnableMaxSat;
    float blendAmountMin;
    float blendAmountMax;
};

displaycolorimetry_t lerp( const displaycolorimetry_t & a, const displaycolorimetry_t & b, float t );
colormapping_t lerp( const colormapping_t & a, const colormapping_t & b, float t );

struct tonemapping_t
{
	bool bUseShaper = true;
	float g22_luminance = 1.f; // what luminance should be applied for g22 EOTF conversions?
};

struct lut3d_t
{
    int lutEdgeSize = 0;
    std::vector<glm::vec3> data; // R changes fastest
};

bool LoadCubeLut( lut3d_t * lut3d, const char * filename );

// Generate a color transform from the source colorspace, to the dest colorspace,
// nLutSize1d is the number of color entries in the shaper lut
// I.e., for a shaper lut with 256 input colors  nLutSize1d = 256, countof(pRgbxData1d) = 1024
// nLutEdgeSize3d is the number of color entries, per edge, in the 3d lut
// I.e., for a 17x17x17 lut nLutEdgeSize3d = 17, countof(pRgbxData3d) = 19652
//
// If the white points differ, this performs an absolute colorimetric match
// Look luts are optional, but if specified applied in the sourceEOTF space

void calcColorTransform( uint16_t * pRgbxData1d, int nLutSize1d,
	uint16_t * pRgbxData3d, int nLutEdgeSize3d,
	const displaycolorimetry_t & source, EOTF sourceEOTF,
	const displaycolorimetry_t & dest,  EOTF destEOTF,
	const colormapping_t & mapping, const nightmode_t & nightmode, const tonemapping_t & tonemapping,
	const lut3d_t * pLook, float flGain );

int writeRawLut( const char * outputFilename, uint16_t * pData, size_t nSize );

// Build colorimetry and a gamut mapping for the given SDR configuration
void buildSDRColorimetry( displaycolorimetry_t * pColorimetry, colormapping_t *pMapping,
	float flSDRGamutWideness, const displaycolorimetry_t & nativeDisplayOutput );

// Build colorimetry and a gamut mapping for the given PQ configuration
void buildPQColorimetry( displaycolorimetry_t * pColorimetry, colormapping_t *pMapping, const displaycolorimetry_t & nativeDisplayOutput );

// Colormetry helper functions for DRM, kindly taken from Weston:
// https://gitlab.freedesktop.org/wayland/weston/-/blob/main/libweston/backend-drm/kms-color.c
// Licensed under MIT.

// Josh: I changed the asserts to clamps here (going to 0, rather than 1) to deal better with
// bad EDIDs (that have 0'ed out metadata) and naughty clients.

static inline uint16_t
color_xy_to_u16(float v)
{
	//assert(v >= 0.0f);
	//assert(v <= 1.0f);
	v = std::clamp(v, 0.0f, 1.0f);

    // CTA-861-G
    // 6.9.1 Static Metadata Type 1
    // chromaticity coordinate encoding
	return (uint16_t)round(v * 50000.0f);
}

static inline uint16_t
nits_to_u16(float nits)
{
	//assert(nits >= 1.0f);
	//assert(nits <= 65535.0f);
	nits = std::clamp(nits, 0.0f, 65535.0f);

	// CTA-861-G
	// 6.9.1 Static Metadata Type 1
	// max display mastering luminance, max content light level,
	// max frame-average light level
	return (uint16_t)round(nits);
}

static inline uint16_t
nits_to_u16_dark(float nits)
{
	//assert(nits >= 0.0001f);
	//assert(nits <= 6.5535f);
	nits = std::clamp(nits, 0.0f, 6.5535f);

	// CTA-861-G
	// 6.9.1 Static Metadata Type 1
	// min display mastering luminance
	return (uint16_t)round(nits * 10000.0f);
}

static constexpr displaycolorimetry_t displaycolorimetry_steamdeck
{
	.primaries = { { 0.602f, 0.355f }, { 0.340f, 0.574f }, { 0.164f, 0.121f } },
	.white = { 0.3070f, 0.3220f },  // not D65
};

static constexpr displaycolorimetry_t displaycolorimetry_709
{
	.primaries = { { 0.64f, 0.33f }, { 0.30f, 0.60f }, { 0.15f, 0.06f } },
	.white = { 0.3127f, 0.3290f },  // D65
};

// Our "saturated SDR target", per jeremys
static constexpr displaycolorimetry_t displaycolorimetry_widegamutgeneric
{
	.primaries = { { 0.6825f, 0.3165f }, { 0.241f, 0.719f }, { 0.138f, 0.050f } },
	.white = { 0.3127f, 0.3290f },  // D65
};

static constexpr displaycolorimetry_t displaycolorimetry_2020
{
	.primaries = { { 0.708f, 0.292f }, { 0.170f, 0.797f }, { 0.131f, 0.046f } },
	.white = { 0.3127f, 0.3290f },  // D65
};

int color_tests();