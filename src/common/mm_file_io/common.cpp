 /*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IO callback class implementation

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/mm_io_x.h"
#include "common/mm_file_io.h"

void
mm_file_io_c::prepare_path(const std::string &path) {
  boost::filesystem::path directory = boost::filesystem::path(path).parent_path();
  if (directory.empty() || boost::filesystem::exists(directory))
    return;

  boost::system::error_code error_code;
  boost::filesystem::create_directories(directory, error_code);
  if (error_code)
    throw mtx::mm_io::create_directory_x(path, mtx::mm_io::make_error_code());
}

memory_cptr
mm_file_io_c::slurp(std::string const &file_name) {
  mm_file_io_c in(file_name, MODE_READ);

  // Don't try to retrieve the file size in order to enable reading
  // from pseudo file systems such as /proc on Linux.
  auto const chunk_size = 10 * 1024;
  auto content          = memory_c::alloc(chunk_size);
  auto total_size_read  = 0;

  while (true) {
    auto num_read    = in.read(content->get_buffer() + total_size_read, chunk_size);
    total_size_read += num_read;

    if (num_read != chunk_size) {
      content->resize(total_size_read);
      break;
    }

    content->resize(content->get_size() + chunk_size);
  }

  return content;
}

uint64
mm_file_io_c::getFilePointer() {
  return m_current_position;
}

mm_file_io_c::~mm_file_io_c() {
  close();
  m_file_name = "";
}

void
mm_file_io_c::cleanup() {
}

mm_io_cptr
mm_file_io_c::open(const std::string &path,
                   const open_mode mode) {
  return mm_io_cptr(new mm_file_io_c(path, mode));
}