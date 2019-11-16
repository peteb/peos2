scenario "qemu i386 multiboot startup" do
  builds [
    %q(OPTLEVEL=-O0 make clean all),
    %q(OPTLEVEL="-O0 -g" make clean all),
    %q(OPTLEVEL=-O1 make clean all),
    %q(OPTLEVEL=-O2 make clean all),
    %q(OPTLEVEL=-O3 make clean all)
  ]

  command "qemu-system-i386 -nographic -s -kernel kernel/vmpeoz -no-reboot -d pcall,cpu_reset,guest_errors -initrd init.tar"

  it "prints out 'Spawned'" do
    expect <<-EOS
      expect {
        "Spawned" { exit 0 }
      }
    EOS
  end

  it "prints fork somewhere" do
    expect <<-EOS
      expect {
        "fork" { exit 0 }
      }
    EOS
  end
end
