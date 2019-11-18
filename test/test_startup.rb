KERNEL_BUILDS = [
  %q(OPTLEVEL=-O0 GRUB_CFG=grub-test.cfg make clean all image),
  %q(OPTLEVEL="-O0 -g" GRUB_CFG=grub-test.cfg make clean all image),
  %q(OPTLEVEL=-O1 GRUB_CFG=grub-test.cfg make clean all image),
  %q(OPTLEVEL=-O2 GRUB_CFG=grub-test.cfg make clean all image),
  %q(OPTLEVEL=-O3 GRUB_CFG=grub-test.cfg make clean all image)
]

scenario "qemu i386 multiboot startup" do
  builds KERNEL_BUILDS

  command "qemu-system-i386 -nographic -s -kernel kernel/vmpeoz -no-reboot -d pcall,cpu_reset,guest_errors -initrd init.tar -append init=/ramfs/bin/tester"

  it "runs the 'tester' program" do
    expect <<-EOS
      expect "WELCOME TO TESTER" { exit 0 }
      exit 1
    EOS
  end
end

scenario "qemu i386 image startup" do
  builds KERNEL_BUILDS

  command "qemu-system-i386 -cdrom peos2.img -nographic -no-reboot -d pcall,cpu_reset,guest_errors"

  it "runs the 'tester' program" do
    expect <<-EOS
      expect "WELCOME TO TESTER" { exit 0 }
      exit 1
    EOS
  end
end

scenario "bochs i386 image startup" do
  builds KERNEL_BUILDS

  command "bochs -q -f bochsrc.test"

  it "runs the 'tester' program" do
    expect <<-EOS
      expect "WELCOME TO TESTER" { exit 0 }
      exit 1
    EOS
  end
end
