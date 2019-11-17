scenario "qemu i386 multiboot startup" do
  builds [
    %q(OPTLEVEL=-O0 make clean all),
    %q(OPTLEVEL="-O0 -g" make clean all),
    %q(OPTLEVEL=-O1 make clean all),
    %q(OPTLEVEL=-O2 make clean all),
    %q(OPTLEVEL=-O3 make clean all)
  ]

  command "qemu-system-i386 -nographic -s -kernel kernel/vmpeoz -no-reboot -d pcall,cpu_reset,guest_errors -initrd init.tar -append /ramfs/bin/tester"

  it "runs the 'tester' program" do
    expect <<-EOS
      expect "WELCOME TO TESTER" { exit 0 }
      exit 1
    EOS
  end
end
