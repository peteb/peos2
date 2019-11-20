class FileContext
  def initialize
    @scenarios = []
  end

  def scenario(name, &block)
    @scenarios << TestSuiteBuilder.new(name).tap do |suite|
      suite.instance_eval(&block)
    end
  end

  def build
    @scenarios.map(&:build)
  end
end

class TestCaseBuilder
  def initialize(name, command)
    @name = name
    @command = command
  end

  def expect(expect_script)
    @script = expect_script
    @command_class = ExpectCommandRunner
  end

  def execute
    @script = nil
    @command_class = ExecuteCommandRunner
  end

  def build
    TestCase.new(@name, @command_class.new(@command, @script))
  end
end

class TestSuiteBuilder
  def initialize(name)
    @name = name
    @testcases = []
  end

  def it(description, &block)
    raise unless @command

    testcase = TestCaseBuilder.new(description, @command)
    testcase.instance_eval(&block)
    @testcases << testcase
  end

  def build
    TestSuite.new(@name, @builds, @testcases.map(&:build))
  end

  private

  def builds(suite_builds); @builds = suite_builds; end
  def command(suite_command); @command = suite_command; end
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
    IO.popen("expect", "w+", err: File::NULL) { |io| io.write(@script) }
    $?.to_i == 0
  end

  def explain
    puts "expect <<EOF"
    puts @script
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
    system(@command, err: File::NULL, out: File::NULL)
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

class TestSuite
  attr_reader :name, :builds

  #
  # Loads and parses the DSL in the given filename
  #
  def self.load_suites(filename)
    context = FileContext.new
    context.instance_eval(File.read(filename), filename)
    context.build
  end

  #
  # Executes the suite and reports to stdout. Expects
  # the build to have been executed successfully.
  #
  def run
    @test_cases.each do |test|
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

  def initialize(name, builds, test_cases)
    @name = name
    @builds = builds
    @test_cases = test_cases
  end
end

def group_suites_by_build(suites)
  build_suite_pairs = suites.flat_map do |suite|
    suite.builds.map { |build| [build, suite] }
  end

  build_suite_pairs
    .group_by(&:first)
    .map { |build, pairs| [build, pairs.map(&:second) ] }
end

def execute_build(command)
  system(command, err: File::NULL, out: File::NULL)
end
