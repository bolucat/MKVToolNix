/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_avi.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file r_avi.h
    \version $Id$
    \brief class definitions for the AVI demultiplexer module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_DTS_H
#define __R_DTS_H

#include "os.h"

#include <stdio.h>

#include "mm_io.h"
#include "pr_generic.h"
#include "common.h"
#include "error.h"

class dts_reader_c: public generic_reader_c {
private:
  unsigned char *chunk;
  mm_io_c *mm_io;
  int64_t bytes_processed, size;

public:
  dts_reader_c(track_info_c *nti) throw (error_c);
  virtual ~dts_reader_c();

  virtual int read(generic_packetizer_c *ptzr);
  virtual int display_priority();
  virtual void display_progress(bool final = false);
  virtual void identify();

  static int probe_file(mm_io_c *mm_io, int64_t size);
};

#endif // __R_DTS_H
