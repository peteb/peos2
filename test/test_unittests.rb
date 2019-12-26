scenario "libsupport unittest" do
  builds [
    %q(OPT_FLAGS="-O0" make -C support -f Makefile.host clean unittest),
    %q(OPT_FLAGS="-O3" make -C support -f Makefile.host clean unittest),
  ]

  command "support/.host/unittest"

  it "runs successfully" do
    successfully_executes
  end
end

scenario "programs unittest" do
  builds [
    %q(OPT_FLAGS="-O0" make -C programs -f Makefile.host clean unittest),
    %q(OPT_FLAGS="-O3" make -C programs -f Makefile.host clean unittest),
  ]

  command "programs/unittest"

  it "runs successfully" do
    successfully_executes
  end
end
