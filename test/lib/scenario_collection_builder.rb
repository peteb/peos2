class ScenarioCollectionBuilder
  attr_reader :scenario_builders

  def initialize
    @scenario_builders = {}
  end

  def build
    @scenario_builders.values
      .reject(&:template?)
      .flat_map(&:build)
  end

  private

  def scenario(name, &block)
    @scenario_builders[name] = ScenarioBuilder.new(self, name).tap do |suite|
      suite.instance_eval(&block)
    end
  end

  # Util for scenarios
  def make_kernel(env)
    env = {'GRUB_CFG' => 'grub-shell.cfg', 'DEFS' => '-DNODEBUG'}.merge(env)
    joined_env = env.map { |k, v| "#{k}=\"#{v}\"" }.join(' ')
    "#{joined_env} build/setenv target make clean all image"
  end
end
