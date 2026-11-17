// Bass Suite — Guitarix TS9 nonlinear table bridge
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Table and model from Guitarix (GPL-2.0-or-later, Copyright (C) 2012
// Andreas Degert, Hermann Meyer), based on a TS-9 circuit analysis by Tamas
// Kenez.

#include <cmath>

#define __rt_data
#include "ts9nonlin.h"
#undef __rt_data

#include "dsp/ts9.hpp"

namespace bass {

double ts9_nonlinear(double x) noexcept { return ts9nonlin(x); }

} // namespace bass
