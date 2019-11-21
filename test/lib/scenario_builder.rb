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

