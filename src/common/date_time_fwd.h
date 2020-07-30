/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations date/time helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#pragma once

#include "common/common_pch.h"

#include <chrono>

namespace mtx::date_time {

using point_t = std::chrono::time_point<std::chrono::system_clock>;

enum class epoch_timezone_e {
  UTC,
  local,
};

}