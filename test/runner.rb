#!/usr/bin/env ruby

# Tests are aggregated per build to minimize wasteful rebuilding.
# Tests are grouped by a scenario and can verify that a certain
# command succeeded. Tests can also interact with and verify a process
# using Tcl Expect scripts.
# It's important that build names fully encapsulate the information
# needed to create the build. Ie, the build shouldn't depend on any
# "side effects".

require_relative "lib/test_suite.rb"
require_relative "lib/utils.rb"

# All paths are relative to the top of the repository
chdir_top_of_repos

test_suites = Dir["test/test_*.rb"]
                .flat_map { |file| TestSuite.load_suites(file) }

suites_by_build = group_suites_by_build(test_suites)

suites_by_build.each do |build, suites|
  puts  "[------------] ".green + build
  print "[ RUN...     ] ".green + "  build\r"

  if execute_build(build)
    puts "[         OK ] ".green + "  build"
    exit 1 unless suites.all?(&:run)

    puts "[------------] ".green + "all tests passed"
    puts

  else
    puts "[       FAIL ] ".red + "  build"
    puts
    puts "Command that failed:"
    puts build
    exit 1
    break
  end

end
