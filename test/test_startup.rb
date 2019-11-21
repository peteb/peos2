KERNEL_BUILDS = [
  make_kernel('OPT_FLAGS' => '-O0', 'GRUB_CFG' => 'grub-test.cfg'),
  make_kernel('OPT_FLAGS' => '-O0 -g', 'GRUB_CFG' => 'grub-test.cfg'),
  make_kernel('OPT_FLAGS' => '-O1', 'GRUB_CFG' => 'grub-test.cfg'),
  make_kernel('OPT_FLAGS' => '-O2', 'GRUB_CFG' => 'grub-test.cfg'),
  make_kernel('OPT_FLAGS' => '-O3', 'GRUB_CFG' => 'grub-test.cfg')
]

scenario "startup" do
  it "runs the 'tester' program" do
    successfully_expects <<~'EOS'
      expect "WELCOME TO TESTER" { exit 0 }
    EOS
  end
end

scenario "qemu i386 multiboot" do
  builds KERNEL_BUILDS
  command "./run-qemu test-tester"
  it_successfully_runs "startup"
end

scenario "qemu i386 image" do
  builds KERNEL_BUILDS
  command "./run-qemu test-cdrom"
  it_successfully_runs "startup"
end

scenario "bochs i386 image" do
  builds KERNEL_BUILDS
  command "./run-bochs"
  it_successfully_runs "startup"
end
