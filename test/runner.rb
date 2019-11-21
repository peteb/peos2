#!/usr/bin/env ruby

# runner.rb - executes integration tests
#
# Tests...
#   - are aggregated per build to minimize wasteful rebuilding
#   - are grouped by a shell command, forming a scenario or test suite
#   - are declarative: imperative testing is outsourced to commands
#   - can verify that the scenario command succeeded
#   - can interact with and verify the scenario process using Tcl Expect scripts
#
# It's important that build names fully encapsulate the information
# needed to create the build. Ie, the build shouldn't depend on any
# "side effects".

require_relative "lib/test_suite.rb"
require_relative "lib/utils.rb"

# All paths are relative to the top of the repository
chdir_top_of_repos

scenarios = Dir["test/test_*.rb"]
                .flat_map { |file| Scenario.load_scenarios(file) }

scenarios_by_build = group_scenarios_by_build(scenarios)

scenarios_by_build.each do |build, scenarios|
  puts  "[------------] ".green + build
  print "[ RUN...     ] ".green + "  build\r"

  if execute_build(build)
    puts "[         OK ] ".green + "  build"
    exit 1 unless scenarios.all?(&:run)

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
