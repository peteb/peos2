scenario "libsupport unittest" do
  builds [
    %q(OPTLEVEL="-O0" make -C support -f Makefile.host clean unittest),
    %q(OPTLEVEL="-O3" make -C support -f Makefile.host clean unittest),
  ]

  command "support/unittest"

  it "runs successfully" do
    execute
  end
end
