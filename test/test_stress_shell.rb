KERNEL_BUILDS = [
  %q(OPT_FLAGS=-O0 GRUB_CFG=grub-shell.cfg make clean all image),
  %q(OPT_FLAGS="-O0 -g" GRUB_CFG=grub-shell.cfg make clean all image),
  %q(OPT_FLAGS=-O1 GRUB_CFG=grub-shell.cfg make clean all image),
  %q(OPT_FLAGS=-O2 GRUB_CFG=grub-shell.cfg make clean all image),
  %q(OPT_FLAGS=-O3 GRUB_CFG=grub-shell.cfg make clean all image),
  %q(OPT_FLAGS=-O3 GRUB_CFG=grub-shell.cfg NODEBUG=true make clean all image)
]

# TODO: don't use global variables here

$STARTUP_TEST = <<~'EOS'
expect {
  "WELCOME TO SHELL"
  timeout { exit 1 }
}

expect ">" { exit 1 }
EOS

$STRESS_TEST = <<~'EOS'
expect "WELCOME TO SHELL"

for {set i 1} {$i < 200} {incr i 1} {
  expect ">" {
    send "/ramfs/bin/tester\r"
  }

  expect {
    "WELCOME TO TESTER" {}
    "panic: ASSERT" { exit 1 }
  }
}

exit 0
EOS

scenario "qemu i386 multiboot shell stress" do
  builds KERNEL_BUILDS

  command "qemu-system-i386 -nographic -s -kernel kernel/vmpeoz -no-reboot -d pcall,cpu_reset,guest_errors -initrd init.tar -append init=/ramfs/bin/shell"

  it "starts the shell" do
    expect $STARUP_TEST
  end

  it "runs the tester many times" do
    expect $STRESS_TEST
  end
end

scenario "qemu i386 image shell stress" do
  builds KERNEL_BUILDS

  command "qemu-system-i386 -cdrom peos2.img -nographic -no-reboot -d pcall,cpu_reset,guest_errors"

  it "starts the shell" do
    expect $STARUP_TEST
  end

  it "runs the tester many times" do
    expect $STRESS_TEST
  end
end

scenario "bochs i386 image shell stress" do
  builds KERNEL_BUILDS

  command "./run-bochs"

  it "starts the shell" do
    expect $STARUP_TEST
  end

  it "runs the tester many times" do
    expect $STRESS_TEST
  end
end
