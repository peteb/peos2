class TestCase
  attr_reader :name

  def initialize(name, runner)
    @name = name
    @runner = runner
  end

  def run
    @runner.run
  end

  def explain
    @runner.explain
  end
end

#
# A scenario consists of a number of test cases that are run for each
# combination of build and command
#
class Scenario
  attr_reader :name, :builds, :command
  attr_accessor :testcases

  #
  # Loads and parses the DSL in the given filename
  #
  def self.load_scenarios(filename)
    collection = ScenarioCollectionBuilder.new
    collection.instance_eval(File.read(filename), filename)
    collection.build
  end

  #
  # Executes the suite and reports to stdout. Expects
  # the build to have been executed successfully.
  #
  def run
    @testcases.each do |test|
      print "[ RUN...     ] ".green + "  #{name} #{test.name}\r"

      if test.run
        puts  "[         OK ] ".green + "  #{name} #{test.name}"
      else
        puts  "[       FAIL ] ".red + "  #{name} #{test.name}"
        puts
        puts "Command that failed:"
        test.explain
        return false
      end
    end

    true
  end

  private

  def initialize(name, builds, command)
    @name = name
    @builds = builds
    @command = command
  end
end

def group_scenarios_by_build(scenarios)
  build_scenario_pairs = scenarios.flat_map do |scenario|
    scenario.builds.map { |build| [build, scenario] }
  end

  build_scenario_pairs
    .group_by(&:first)
    .map { |build, pairs| [build, pairs.map(&:second) ] }
end

def execute_build(command)
  system(command, err: $stderr, out: File::NULL)
end
