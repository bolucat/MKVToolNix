#!/usr/bin/ruby -w

# T_710fuzzying
describe "mkvmerge / issues found by fuzzying"

Dir["data/segfaults-assertions/fuzzying/0001-ac3/**/id*"].each do |file|
  test file do
    expected_exit_code = file.gsub(%r{.*/([012])/.*}, '\1').to_i

    _, actual_exit_code = *merge(file, :exit_code => expected_exit_code, :no_variable_data => true)

    actual_exit_code == expected_exit_code ? "ok" : "bad"
  end
end