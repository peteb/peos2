#
# Creates a test case that runs a command runner, for example Tcl Expect
#
class CommandRunnerTestCaseBuilder
  def initialize(name)
    @name = name
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

  private

  def successfully_expects(expect_script)
    @script = expect_script
    @command_class = ExpectCommandRunner
  end

  def successfully_executes
    @script = nil
    @command_class = ExecuteCommandRunner
  end
end

#
# Works like an ordinary test case builder but returns builders from
# another scenario
#
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