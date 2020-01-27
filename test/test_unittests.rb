scenario "libsupport unittest" do
  builds [
    %q(OPT_FLAGS="-O0" setenv host make clean unittest),
    %q(OPT_FLAGS="-O3" setenv host make clean unittest),
  ]

  # TODO: remove hard coded host triplet
  command "libraries/support/.x86_64-linux-gnu/unittest"

  it "runs successfully" do
    successfully_executes
  end
end

scenario "libnet unittest" do
  builds [
    %q(OPT_FLAGS="-O0" setenv host make clean unittest),
    %q(OPT_FLAGS="-O3" setenv host make clean unittest),
  ]

  # TODO: remove hard coded host triplet
  command "libraries/net/.x86_64-linux-gnu/unittest"

  it "runs successfully" do
    successfully_executes
  end
end

