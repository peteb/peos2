scenario "libsupport unittest" do
  builds [
    %q(OPT_FLAGS="-O0" make -C support -f Makefile.host clean all),
    %q(OPT_FLAGS="-O3" make -C support -f Makefile.host clean all),
  ]

  command "support/.host/unittest"

  it "runs successfully" do
    successfully_executes
  end
end
