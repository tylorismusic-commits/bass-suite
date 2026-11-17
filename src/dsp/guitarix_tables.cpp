// Bass Suite — Guitarix tube table bridge
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Includes Guitarix's generated tube tables exactly once.  The tables and
// lookup functions are GPL-2.0-or-later, Copyright (C) 2012 Hermann Meyer,
// Andreas Degert, Pete Shorthose, Steve Poskitt.

#include <cstdint>

#define __rt_data
#include "valve.h"

#include "dsp/tube_stage.hpp"

namespace bass {

double guitarix_ftube(int table, double vgk) noexcept {
  return Ftube(table, vgk);
}

double guitarix_ranode(int table, double vgk) noexcept {
  return Ranode(table, vgk);
}

static_assert(static_cast<int>(TubeTable::k12AX7_68k) ==
                  TUBE_TABLE_12AX7_68k,
              "tube table enum order drifted");
static_assert(static_cast<int>(TubeTable::k12AX7_250k) ==
                  TUBE_TABLE_12AX7_250k,
              "tube table enum order drifted");
static_assert(static_cast<int>(TubeTable::kEL34_68k) == TUBE_TABLE_EL34_68k,
              "tube table enum order drifted");
static_assert(static_cast<int>(TubeTable::kEL84_68k) == TUBE_TABLE_EL84_68k,
              "tube table enum order drifted");
static_assert(static_cast<int>(TubeTable::k6L6CG_68k) == TUBE_TABLE_6L6CG_68k,
              "tube table enum order drifted");
static_assert(static_cast<int>(TubeTable::k6V6_68k) == TUBE_TABLE_6V6_68k,
              "tube table enum order drifted");
static_assert(static_cast<int>(TubeTable::Count) == TUBE_TABLE_SIZE,
              "tube table enum count drifted");

} // namespace bass
