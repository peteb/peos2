class ScenarioCollectionBuilder
  attr_reader :scenario_builders

  def initialize
    @scenario_builders = {}
  end

  def scenario(name, &block)
    @scenario_builders[name] = ScenarioBuilder.new(self, name).tap do |suite|
      suite.instance_eval(&block)
    end
  end

  def build
    @scenario_builders.values
      .reject(&:template?)
      .flat_map(&:build)
  end

  private

  # Util for scenarios
  def make_kernel(env)
    env = {'GRUB_CFG' => 'grub-shell.cfg', 'DEFS' => '-DNODEBUG'}.merge(env)
    joined_env = env.map { |k, v| "#{k}=\"#{v}\"" }.join(' ')
    "#{joined_env} make clean all image"
  end
end

class TestCaseBuilder
  def initialize(name)
    @name = name
  end

  def successfully_expects(expect_script)
    @script = expect_script
    @command_class = ExpectCommandRunner
  end

  def successfully_executes
    @script = nil
    @command_class = ExecuteCommandRunner
  end

  def build(scenario, **opts)
    raise "Missing command" unless scenario.command

    imbued_name = if opts[:group]
                    "#{@name} \t[#{opts[:group]}]"
                  else
                    @name
                  end

    TestCase.new(imbued_name, @command_class.new(scenario.command, @script))
  end
end

# Works like an ordinary test case builder but returns builders from
# another scenario
class ExternalTestCasesBuilder
  def initialize(scenario_builder, scenario_name)
    @scenario_builder = scenario_builder
    @scenario_name = scenario_name
  end

  def build(scenario, **opts)
    # NB: this will loop forever in the presence of cycles
    target_scenario_builder = @scenario_builder.fetch_sibling_builder(@scenario_name)
    target_scenario_builder.testcases.flat_map { |tc| tc.build(scenario, group: @scenario_name) }
  end
end

class ScenarioBuilder
  attr_reader :testcases
  attr_reader :scenario_collection_builder

  def initialize(scenario_collection_builder, name)
    @scenario_collection_builder = scenario_collection_builder
    @name = name
    @testcases = []
    @external_scenarios = []
  end

  def it(description, &block)
    testcase = TestCaseBuilder.new(description)
    testcase.instance_eval(&block)
    @testcases << testcase
  end

  def it_successfully_runs(other_scenario)
    @testcases << ExternalTestCasesBuilder.new(self, other_scenario)
  end

  def build
    scenario = Scenario.new(@name, @builds, @command)
    scenario.testcases = @testcases.flat_map { |tc| tc.build(scenario) }
    scenario
  end

  def template?
    @command.nil? || @builds.nil?
  end

  def fetch_sibling_builder(scenario_name)
    scenario_collection_builder.scenario_builders.fetch(scenario_name)
  end

  private

  def builds(scenario_builds); @builds = scenario_builds; end
  def command(scenario_command); @command = scenario_command; end
end

#
# Executes the given command within a Tcl Expect script (given as `script`)
#
class ExpectCommandRunner
  def initialize(command, script)
    @script = <<~EOS
      set timeout 5
      log_user 0

      spawn #{command}

      expect_before {
        timeout { exit 1 }
        eof { exit 1 }
      }

      #{script}
    EOS
  end

  def run
    IO.popen("expect", "w+") { |io| io.write(@script) }
    $?.to_i == 0
  end

  def explain
    puts "expect <<EOF"
    puts @script.gsub('$', '\$')
    puts "EOF"
  end
end

#
# Executes the command and expects a successful exit status
#
class ExecuteCommandRunner
  def initialize(command, script)
    @command = command
  end

  def run
    system(@command, out: File::NULL)
    $? == 0
  end

  def explain
    puts @command
  end
end

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
  system(command, err: File::NULL, out: File::NULL)
end
