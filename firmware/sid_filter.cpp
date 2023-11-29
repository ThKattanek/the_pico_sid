//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_filter.cpp                        //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#include "./sid_filter.h"

fc_point SID_FILTER::f0_points_6581[] =
{
  //  FC      f         FCHI FCLO
  // ----------------------------
  {    0,   220 },   // 0x00      - repeated end point
  {    0,   220 },   // 0x00
  {  128,   230 },   // 0x10
  {  256,   250 },   // 0x20
  {  384,   300 },   // 0x30
  {  512,   420 },   // 0x40
  {  640,   780 },   // 0x50
  {  768,  1600 },   // 0x60
  {  832,  2300 },   // 0x68
  {  896,  3200 },   // 0x70
  {  960,  4300 },   // 0x78
  {  992,  5000 },   // 0x7c
  { 1008,  5400 },   // 0x7e
  { 1016,  5700 },   // 0x7f
  { 1023,  6000 },   // 0x7f 0x07
  { 1023,  6000 },   // 0x7f 0x07 - discontinuity
  { 1024,  4600 },   // 0x80      -
  { 1024,  4600 },   // 0x80
  { 1032,  4800 },   // 0x81
  { 1056,  5300 },   // 0x84
  { 1088,  6000 },   // 0x88
  { 1120,  6600 },   // 0x8c
  { 1152,  7200 },   // 0x90
  { 1280,  9500 },   // 0xa0
  { 1408, 12000 },   // 0xb0
  { 1536, 14500 },   // 0xc0
  { 1664, 16000 },   // 0xd0
  { 1792, 17100 },   // 0xe0
  { 1920, 17700 },   // 0xf0
  { 2047, 18000 },   // 0xff 0x07
  { 2047, 18000 }    // 0xff 0x07 - repeated end point
};

fc_point SID_FILTER::f0_points_8580[] =
{
  //  FC      f         FCHI FCLO
  // ----------------------------
  {    0,     0 },   // 0x00      - repeated end point
  {    0,     0 },   // 0x00
  {  128,   800 },   // 0x10
  {  256,  1600 },   // 0x20
  {  384,  2500 },   // 0x30
  {  512,  3300 },   // 0x40
  {  640,  4100 },   // 0x50
  {  768,  4800 },   // 0x60
  {  896,  5600 },   // 0x70
  { 1024,  6500 },   // 0x80
  { 1152,  7500 },   // 0x90
  { 1280,  8400 },   // 0xa0
  { 1408,  9200 },   // 0xb0
  { 1536,  9800 },   // 0xc0
  { 1664, 10500 },   // 0xd0
  { 1792, 11000 },   // 0xe0
  { 1920, 11700 },   // 0xf0
  { 2047, 12500 },   // 0xff 0x07
  { 2047, 12500 }    // 0xff 0x07 - repeated end point
};

SID_FILTER::SID_FILTER()
{
	fc = 0;
	res = 0;
	filt = 0;
	voice3off = 0;
	hp_bp_lp = 0;
	vol = 0;

  	// State of filter.
  	Vhp = 0;
  	Vbp = 0;
  	Vlp = 0;
  	Vnf = 0;

  	EnableFilter(true);

  	// Create mappings from FC to cutoff frequency.
  	interpolate(f0_points_6581, f0_points_6581 + sizeof(f0_points_6581)/sizeof(*f0_points_6581) - 1, PointPlotter<int>(f0_6581), 1.0);
  	interpolate(f0_points_8580, f0_points_8580 + sizeof(f0_points_8580)/sizeof(*f0_points_8580) - 1, PointPlotter<int>(f0_8580), 1.0);

	SetSidType(MOS_6581);
}

SID_FILTER::~SID_FILTER()
{

}

void SID_FILTER::EnableFilter(bool enable)
{
	enabled = enable;
}

void SID_FILTER::SetSidType(sid_type type)
{
 	if (type == MOS_6581) 
 	{
		// The mixer has a small input DC offset. This is found as follows:
		//
		// The "zero" output level of the mixer measured on the SID audio
		// output pin is 5.50V at zero volume, and 5.44 at full
		// volume. This yields a DC offset of (5.44V - 5.50V) = -0.06V.
		//
		// The DC offset is thus -0.06V/1.05V ~ -1/18 of the dynamic range
		// of one voice. See voice.cc for measurement of the dynamic
		// range.

    	mixer_DC = -0xfff*0xff/18 >> 7;

    	f0 = f0_6581;
    	f0_points = f0_points_6581;
    	f0_count = sizeof(f0_points_6581)/sizeof(*f0_points_6581);
  	}
  	else 
	{
		// No DC offsets in the MOS8580.
		mixer_DC = 0;

		f0 = f0_8580;
		f0_points = f0_points_8580;
		f0_count = sizeof(f0_points_8580)/sizeof(*f0_points_8580);
  	}

	SetW0();
	SetQ();
}

void SID_FILTER::Reset()
{
	fc = 0;

	res = 0;

	filt = 0;

	voice3off = 0;

	hp_bp_lp = 0;

	vol = 0;

	// State of filter.
	Vhp = 0;
	Vbp = 0;
	Vlp = 0;
	Vnf = 0;

	SetW0();
	SetQ();
}

void SID_FILTER::WriteFcLo(reg8 value)
{
	fc = (fc & 0x7f8) | (value & 0x007);
	SetW0();
}

void SID_FILTER::WriteFcHi(reg8 value)
{
	fc = ((value << 3) & 0x7f8) | (fc & 0x007);
	SetW0();
}

void SID_FILTER::WriteResFilt(reg8 value)
{
	res = (value >> 4) & 0x0f;
	SetQ();
	filt = value & 0x0f;
}

void SID_FILTER::WriteModeVol(reg8 value)
{
	voice3off = value & 0x80;
	hp_bp_lp = (value >> 4) & 0x07;
	vol = value & 0x0f;
}

// Set filter cutoff frequency.
void SID_FILTER::SetW0()
{
  const double pi = 3.1415926535897932385;

  // Multiply with 1.048576 to facilitate division by 1 000 000 by right-
  // shifting 20 times (2 ^ 20 = 1048576).
  w0 = static_cast<int>(2*pi*f0[fc]*1.048576);

  // Limit f0 to 16kHz to keep 1 cycle filter stable.
  const int w0_max_1 = static_cast<int>(2*pi*16000*1.048576);
  w0_ceil_1 = w0 <= w0_max_1 ? w0 : w0_max_1;

  // Limit f0 to 4kHz to keep delta_t cycle filter stable.
  const int w0_max_dt = static_cast<int>(2*pi*4000*1.048576);
  w0_ceil_dt = w0 <= w0_max_dt ? w0 : w0_max_dt;
}

// Set filter resonance.
void SID_FILTER::SetQ()
{
  // Q is controlled linearly by res. Q has approximate range [0.707, 1.7].
  // As resonance is increased, the filter must be clocked more often to keep
  // stable.

  // The coefficient 1024 is dispensed of later by right-shifting 10 times
  // (2 ^ 10 = 1024).
  _1024_div_Q = static_cast<int>(1024.0/(0.707 + 1.0*res/0x0f));
}

// ----------------------------------------------------------------------------
// Spline functions.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Return the array of spline interpolation points used to map the FC register
// to filter cutoff frequency.
// ----------------------------------------------------------------------------
void SID_FILTER::FcDefault(const fc_point*& points, int& count)
{
  points = f0_points;
  count = f0_count;
}

// ----------------------------------------------------------------------------
// Given an array of interpolation points p with n points, the following
// statement will specify a new FC mapping:
//   interpolate(p, p + n - 1, filter.fc_plotter(), 1.0);
// Note that the x range of the interpolation points *must* be [0, 2047],
// and that additional end points *must* be present since the end points
// are not interpolated.
// ----------------------------------------------------------------------------
PointPlotter<int> SID_FILTER::FcPlotter()
{
  return PointPlotter<int>(f0);
}