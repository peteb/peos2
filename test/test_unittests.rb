scenario "libsupport unittest" do
  builds [
    %q(OPT_FLAGS="-O0" build/setenv host make clean unittest),
    %q(OPT_FLAGS="-O3" build/setenv host make clean unittest),
  ]

  # TODO: remove hard coded host triplet
  command "support/.x86_64-linux-gnu/unittest"

  it "runs successfully" do
    successfully_executes
  end
end

scenario "programs unittest" do
  builds [
    %q(OPT_FLAGS="-O0" build/setenv host make clean unittest),
    %q(OPT_FLAGS="-O3" build/setenv host make clean unittest),
  ]

  command "programs/unittest"

  it "runs successfully" do
    successfully_executes
  end
end
