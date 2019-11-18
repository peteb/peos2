scenario "libsupport unittest" do
  builds [
    %q(OPT_FLAGS="-O0" make -C support -f Makefile.host clean unittest),
    %q(OPT_FLAGS="-O3" make -C support -f Makefile.host clean unittest),
  ]

  command "support/unittest"

  it "runs successfully" do
    execute
  end
end
