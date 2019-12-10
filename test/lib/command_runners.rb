#
# Executes the given command within a Tcl Expect script given as
# @script. The script should return 0 if successful and != 1 on
# failure
#
class ExpectCommandRunner
  def initialize(command, script)
    @script = <<~EOS
      set timeout 10
      log_user 1

      spawn #{command}

      expect_before {
        timeout { exit 1 }
      }

      #{script}
    EOS
  end

  def run
    IO.popen("expect", "w+") do |io|
      io.write(@script)
      io.close_write

      if ENV.fetch('OUTPUT', 'NO') != 'NO'
        io.each_line { |line| puts line }
      end
    end

    $?.to_i == 0
  end

  def explain
    puts "expect <<EOF"
    puts @script.gsub('$', '\$')
    puts "EOF"
  end
end

#
# Executes the given command and expects a successful exit status
#
class ExecuteCommandRunner
  def initialize(command, script)
    @command = command
  end

  def run
    system(@command, out: File::NULL)
    $? == 0
  end

  def explain
    puts @command
  end
end
