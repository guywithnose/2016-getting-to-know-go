#!/usr/bin/env ruby
require 'io/console'
version_number = "1.3.1"

# Loads the ncurses-ruby module and imports "Ncurses" into the
# current namespace. It stops the program if loading the
# ncurses-ruby module fails.
def load_ncurses
  begin
    require "ncurses"
    include Ncurses
  rescue LoadError
    $stderr.print <<EOF
  There is no Ncurses-Ruby package installed which is needed by TPP.
  You can download it on: http://ncurses-ruby.berlios.de/
EOF
    Kernel.exit(1)
  end
end

# Maps color names to constants and indexes.
class ColorMap

  # Maps color name _color_ to a constant
  def ColorMap.get_color(color)
    colors = { "white" => COLOR_WHITE,
               "yellow" => COLOR_YELLOW,
               "red" => COLOR_RED,
               "green" => COLOR_GREEN,
               "blue" => COLOR_BLUE,
               "cyan" => COLOR_CYAN,
               "magenta" => COLOR_MAGENTA,
               "black" => COLOR_BLACK,
               "default" => -1 }
    colors[color]
  end

  # Maps color name to a color pair index
  def ColorMap.get_color_pair(color)
    colors = { "white" => 1,
               "yellow" => 2,
               "red" => 3,
               "green" => 4,
               "blue" => 5,
               "cyan" => 6,
               "magenta" => 7,
               "black" => 8,
               "default" =>-1}
    colors[color]
  end

end

# Opens a TPP source file, and splits it into the different pages.
class FileParser

  def initialize(filename)
    @filename = filename
    @pages = []
  end

  # Parses the specified file and returns an array of Page objects
  def get_pages
    begin
      f = File.open(@filename)
    rescue
      $stderr.puts "Error: couldn't open file: #{$!}"
      Kernel.exit(1)
    end

    number_pages = 0

    cur_page = Page.new("")

    f.each_line do |line|
      line.chomp!
      case line
        when /^--##/ # ignore comments
        when /^--newpage/
          @pages << cur_page
          number_pages += 1
          name = line.sub(/^--newpage/,"")
          if name == "" then
            name = "slide " + (number_pages+1).to_s
          else
            name.strip!
          end
          cur_page = Page.new(name)
        else
          cur_page.add_line(line)
      end # case
    end # each
    @pages << cur_page
  end
end # class FileParser


# Represents a page (aka `slide') in TPP. A page consists of a title and one or
# more lines.
class Page

  def initialize(title)
    @lines = []
    if title != ""
      @lines << '--heading ' + title
    end

    @title = title
    if title == ""
      @title = "Title"
    end

    @cur_line = 0
    @eop = false
  end

  # Appends a line to the page, but only if _line_ is not null
  def add_line(line)
    @lines << line if line
    if line =~ /^\$\$/ or line =~ /^\$\%/
      prefix = ''
      prefix = '%' if line =~ /^\$\%/

      cmd = line[2..-1]
      begin
        op = IO.popen(cmd,"r")
        op.readlines.each do |out_line|
          @lines << prefix + out_line
        end
        op.close
      rescue => e
        @lines << e.to_s
      end
    end
  end

  # Returns the next line. In case the last line is hit, then the end-of-page marker is set.
  def next_line
    line = @lines[@cur_line]
    @cur_line += 1
    if @cur_line >= @lines.size then
      @eop = true
    end
    return line
  end

  # Returns whether end-of-page has been reached.
  def eop?
    @eop
  end

  # Resets the end-of-page marker and sets the current line marker to the first line
  def reset_eop
    @cur_line = 0
    @eop = false
  end

  # Returns all lines in the page.
  def lines
    @lines
  end

  # Returns the page's title
  def title
    @title
  end
end

# Implements an interactive visualizer which builds on top of ncurses.
class NcursesVisualizer

  def initialize
    @figletfont = "standard"
    Ncurses.initscr
    Ncurses.curs_set(0)
    Ncurses.cbreak # unbuffered input
    Ncurses.noecho # turn off input echoing
    Ncurses.stdscr.intrflush(false)
    Ncurses.stdscr.keypad(true)
    @screen = Ncurses.stdscr
    @lastFileName = nil
    setsizes
    Ncurses.start_color()
    Ncurses.use_default_colors()
    do_bgcolor("black")
    @fgcolor = ColorMap.get_color_pair("white")
    @voffset = 1
    @indent = 3
    @cur_line = @voffset
    @OUTPUT = @shelloutput = false
  end

  def get_key
    ch = Ncurses.getch
    case ch
      when 100, #d
        68, #D
        106, #j
        74, #J
        108, #l
        76, #L
        Ncurses::KEY_DOWN,
        Ncurses::KEY_RIGHT
        return :keyright
      when 97, #a
        65, #A
        98, #b
        66, #B
        104, #h
        72, #h
        107, #k
        75, #k
        Ncurses::KEY_UP,
        Ncurses::KEY_LEFT
        return :keyleft
      when 122, #z
        90 #Z
        return :keyresize
      when 114, #r
        82 #R
        return :reload
      when 113, #q
        81 #Q
        return :quit
      when 115, #s
        83 #S
        return :firstpage
      when 101, #e
        69 #E
        return :edit
      when 103, #g
        71 #g
        return :jumptoslide
      when 63 #?
        return :help
      when 410 #Window resize?
        return :reload

      else
        return :keyright
    end
  end

  def clear
    @screen.clear
    @screen.refresh
  end


  def setsizes
    @termwidth = Ncurses.getmaxx(@screen)
    @termheight = Ncurses.getmaxy(@screen)
  end

  def do_refresh
    @screen.refresh
  end

  def draw_border
    @screen.move(0,0)
    @screen.addstr(".")
    (@termwidth-2).times { @screen.addstr("-") }; @screen.addstr(".")
    @screen.move(@termheight-2,0)
    @screen.addstr("`")
    (@termwidth-2).times { @screen.addstr("-") }; @screen.addstr("'")
    1.upto(@termheight-3) do |y|
      @screen.move(y,0)
      @screen.addstr("|")
    end
    1.upto(@termheight-3) do |y|
      @screen.move(y,@termwidth-1)
      @screen.addstr("|")
    end
  end

  def new_page(title)
    @cur_line = @voffset
    @output = @shelloutput = false
    setsizes
    @screen.clear
  end

  def do_heading(line)
    @screen.attron(Ncurses::A_BOLD)
    print_heading(line)
    @screen.attroff(Ncurses::A_BOLD)
  end

  def do_horline
    @screen.attron(Ncurses::A_BOLD)
    @termwidth.times do |x|
      @screen.move(@cur_line,x)
      @screen.addstr("-")
    end
    @screen.attroff(Ncurses::A_BOLD)
  end

  def print_heading(text)
    width = @termwidth - 2*@indent
    lines = split_lines(text,width)
    lines.each do |l|
      @screen.move(@cur_line,@indent)
      x = (@termwidth - l.length)/2
      @screen.move(@cur_line,x)
      @screen.addstr(l)
      @cur_line += 1
    end
  end

  def do_center(text)
    width = @termwidth - 2*@indent
    if @output or @shelloutput then
      width -= 2
    end
    lines = split_lines(text,width)
    lines.each do |l|
      @screen.move(@cur_line,@indent)
      if @output or @shelloutput then
        @screen.addstr("| ")
      end
      x = (@termwidth - l.length)/2
      @screen.move(@cur_line,x)
      @screen.addstr(l)
      if @output or @shelloutput then
        @screen.move(@cur_line,@termwidth - @indent - 2)
        @screen.addstr(" |")
      end
      @cur_line += 1
    end
  end

  def do_right(text)
    width = @termwidth - 2*@indent
    if @output or @shelloutput then
      width -= 2
    end
    lines = split_lines(text,width)
    lines.each do |l|
      @screen.move(@cur_line,@indent)
      if @output or @shelloutput then
        @screen.addstr("| ")
      end
      x = (@termwidth - l.length - 5)
      @screen.move(@cur_line,x)
      @screen.addstr(l)
      if @output or @shelloutput then
        @screen.addstr(" |")
      end
      @cur_line += 1
    end
  end

  def show_help_page
    help_text = [ "tpp help",
                  "",
                  "space bar ............................... display next entry within page",
                  "space bar, cursor-down, cursor-right .... display next page",
                  "b, cursor-up, cursor-left ............... display previous page",
                  "q, Q .................................... quit tpp",
                  "j, J .................................... jump directly to page",
                  "l, L .................................... reload current file",
                  "s, S .................................... jump to the first page",
                  "e, E .................................... jump to the last page",
                  "c, C .................................... start command line",
                  "?, h .................................... this help screen" ]
    @screen.clear
    y = @voffset
    help_text.each do |line|
      @screen.move(y,@indent)
      @screen.addstr(line)
      y += 1
    end
    @screen.move(@termheight - 2, @indent)
    @screen.addstr("Press any key to return to slide")
    @screen.refresh
  end

  def do_exec(cmdline)
    Kernel.system(cmdline)
  end

  def do_beginoutput
    @screen.move(@cur_line,@indent)
    @screen.addstr(".")
    (@termwidth - @indent*2 - 2).times { @screen.addstr("-") }
    @screen.addstr(".")
    @output = true
    @cur_line += 1
  end

  def do_beginshelloutput
    @screen.move(@cur_line,@indent)
    @screen.addstr(".")
    (@termwidth - @indent*2 - 2).times { @screen.addstr("-") }
    @screen.addstr(".")
    @shelloutput = true
    @cur_line += 1
  end

  def do_endoutput
    if @output then
      @screen.move(@cur_line,@indent)
      @screen.addstr("`")
      (@termwidth - @indent*2 - 2).times { @screen.addstr("-") }
      @screen.addstr("'")
      @output = false
      @cur_line += 1
    end
  end

  def do_title(title)
    do_boldon
    do_center(title)
    do_boldoff
    do_center("")
  end

  def do_footer(footer_txt)
    @screen.move(@termheight - 2, @termwidth - footer_txt.length - 2)
    @screen.addstr(footer_txt)
  end

 def do_header(header_txt)
    @screen.move(@termheight - @termheight+1, (@termwidth - header_txt.length)/2)
    @screen.addstr(header_txt)
 end

  def do_author(author)
    do_center(author)
    do_center("")
  end

  def do_date(date)
    do_center(date)
    do_center("")
  end

  def do_endshelloutput
    if @shelloutput then
      @screen.move(@cur_line,@indent)
      @screen.addstr("`")
      (@termwidth - @indent*2 - 2).times { @screen.addstr("-") }
      @screen.addstr("'")
      @shelloutput = false
      @cur_line += 1
    end
  end

  def do_sleep(time2sleep)
    Kernel.sleep(time2sleep.to_i)
  end

  def do_boldon
    @screen.attron(Ncurses::A_BOLD)
  end

  def do_boldoff
    @screen.attroff(Ncurses::A_BOLD)
  end

  def do_revon
    @screen.attron(Ncurses::A_REVERSE)
  end

  def do_revoff
    @screen.attroff(Ncurses::A_REVERSE)
  end

  def do_ulon
    @screen.attron(Ncurses::A_UNDERLINE)
  end

  def do_uloff
    @screen.attroff(Ncurses::A_UNDERLINE)
  end

  def do_beginslideleft
    @slideoutput = true
    @slidedir = "left"
  end

  def do_endslide
    @slideoutput = false
  end

  def do_beginslideright
    @slideoutput = true
    @slidedir = "right"
  end

  def do_beginslidetop
    @slideoutput = true
    @slidedir = "top"
  end

  def do_beginslidebottom
    @slideoutput = true
    @slidedir = "bottom"
  end

  def do_sethugefont(params)
    @figletfont = params
  end

  def do_huge(figlet_text)
    output_width = @termwidth - @indent
    output_width -= 2 if @output or @shelloutput
    op = IO.popen("figlet -f #{@figletfont} -w #{output_width} -k \"#{figlet_text}\"","r")
    op.readlines.each do |line|
      print_line(line)
    end
    op.close
  end

  def do_bgcolor(color)
    bgcolor = ColorMap.get_color(color) or COLOR_BLACK
    Ncurses.init_pair(1, COLOR_WHITE, bgcolor)
    Ncurses.init_pair(2, COLOR_YELLOW, bgcolor)
    Ncurses.init_pair(3, COLOR_RED, bgcolor)
    Ncurses.init_pair(4, COLOR_GREEN, bgcolor)
    Ncurses.init_pair(5, COLOR_BLUE, bgcolor)
    Ncurses.init_pair(6, COLOR_CYAN, bgcolor)
    Ncurses.init_pair(7, COLOR_MAGENTA, bgcolor)
    Ncurses.init_pair(8, COLOR_BLACK, bgcolor)
    if @fgcolor then
      Ncurses.bkgd(Ncurses.COLOR_PAIR(@fgcolor))
    else
      Ncurses.bkgd(Ncurses.COLOR_PAIR(1))
    end
  end

  def do_fgcolor(color)
    @fgcolor = ColorMap.get_color_pair(color)
    Ncurses.attron(Ncurses.COLOR_PAIR(@fgcolor))
  end

  def do_color(color)
    num = ColorMap.get_color_pair(color)
    Ncurses.attron(Ncurses.COLOR_PAIR(num))
  end

  def type_line(l)
    l.each_byte do |x|
      @screen.addstr(x.chr)
      @screen.refresh()
      r = rand(20)
      time_to_sleep = (2 + r).to_f / 1000;
      Kernel.sleep(time_to_sleep)
    end
  end

  def slide_text(l)
    return if l == ""
    case @slidedir
    when "left"
      xcount = l.length-1
      while xcount >= 0
        @screen.move(@cur_line,@indent)
        @screen.addstr(l[xcount..l.length-1])
        @screen.refresh()
        time_to_sleep = 1.to_f / 20
        Kernel.sleep(time_to_sleep)
        xcount -= 1
      end
    when "right"
      (@termwidth - @indent).times do |pos|
        @screen.move(@cur_line,@termwidth - pos - 1)
        @screen.clrtoeol()
        @screen.addstr(l[0..pos])
        @screen.refresh()
        time_to_sleep = 1.to_f / 20
        Kernel.sleep(time_to_sleep)
      end # do
    when "top"
      new_scr = @screen.dupwin
      1.upto(@cur_line) do |i|
        Ncurses.overwrite(new_scr,@screen) # overwrite @screen with new_scr
        @screen.move(i,@indent)
        @screen.addstr(l)
        @screen.refresh()
        Kernel.sleep(1.to_f / 10)
      end
    when "bottom"
      new_scr = @screen.dupwin
      (@termheight-1).downto(@cur_line) do |i|
        Ncurses.overwrite(new_scr,@screen)
        @screen.move(i,@indent)
        @screen.addstr(l)
        @screen.refresh()
        Kernel.sleep(1.to_f / 10)
      end
    end
  end

  def print_line(line)
    width = @termwidth - 2*@indent
    if @output or @shelloutput then
      width -= 2
    end
    lines = split_lines(line,width)
    lines.each do |l|
      @screen.move(@cur_line,@indent)
      if (@output or @shelloutput) and ! @slideoutput then
        @screen.addstr("| ")
      end
      if @shelloutput then # allow sh and csh style prompts
        type_line(l)
      elsif @slideoutput then
        slide_text(l)
      else
        @screen.addstr(l)
      end
      if (@output or @shelloutput) and ! @slideoutput then
        @screen.move(@cur_line,@termwidth - @indent - 2)
        @screen.addstr(" |")
      end
      @cur_line += 1
    end
  end

  def close
    Ncurses.nocbreak
    Ncurses.endwin
  end

  def read_newpage(pages,current_page)
    page = []
    @screen.clear()
    col = 0
    line = 2
    pages.each_index do |i|
      @screen.move(line,col*15 + 2)
      if current_page == i then
        @screen.printw("%2d %s <=",i+1,pages[i].title[0..80])
      else
        @screen.printw("%2d %s",i+1,pages[i].title[0..80])
      end
      line += 1
      if line >= @termheight - 3 then
        line = 2
        col += 1
      end
    end
    prompt = "jump to slide: "
    prompt_indent = 12
    @screen.move(@termheight - 2, @indent + prompt_indent)
    @screen.addstr(prompt)
    Ncurses.echo
    @screen.scanw("%d",page)
    Ncurses.noecho
    @screen.move(@termheight - 2, @indent + prompt_indent)
    (prompt.length + page[0].to_s.length).times { @screen.addstr(" ") }
    if page[0] then
      return page[0] - 1
    end
    return -1 # invalid page
  end

  def store_screen
    @screen.dupwin
  end

  def getLastFile
    @lastFileName
  end

  def restore_screen(s)
    Ncurses.overwrite(s,@screen)
  end

  def draw_slidenum(cur_page,max_pages,eop)
    @screen.move(@termheight - 2, @indent)
    @screen.attroff(Ncurses::A_BOLD) # this is bad
    @screen.addstr("[slide #{cur_page}/#{max_pages}]")
    if @footer_txt.to_s.length > 0 then
      do_footer(@footer_txt)
    end
    if @header_txt.to_s.length > 0 then
      do_header(@header_txt)
    end

    if eop then
      draw_eop_marker
    end
  end

  def draw_eop_marker
    @screen.move(@termheight - 2, @indent - 1)
    @screen.attron(A_BOLD)
    @screen.addstr("*")
    @screen.attroff(A_BOLD)
  end

  # Splits a line into several lines, where each of the result lines is at most
  # _width_ characters long, caring about word boundaries, and returns an array
  # of strings.
  def split_lines(text,width)
    lines = []
    if text then
      begin
        i = width
        if text.length <= i then # text length is OK -> add it to array and stop splitting
          lines << text
          text = ""
        else
          # search for word boundary (space actually)
          while i > 0 and text[i] != ' '[0] do
            i -= 1
          end
          # if we can't find any space character, simply cut it off at the maximum width
          if i == 0 then
            i = width
          end
          # extract line
          x = text[0..i-1]
          # remove extracted line
          text = text[i+1..-1]
          # added line to array
          lines << x
        end
      end while text.length > 0
    end
    return lines
  end

  # Receives a _line_, parses it if necessary, and dispatches it
  # to the correct method which then does the correct processing.
  # It returns whether the controller shall wait for input.
  def visualize(line)
    case line
      when /^--heading /
        text = line.sub(/^--heading /,"")
        do_heading(text)
      when /^--withborder/
        draw_border
      when /^--horline/
        do_horline
      when /^--color /
        text = line.sub(/^--color /,"")
        text.strip!
        do_color(text)
      when /^--center /
        text = line.sub(/^--center /,"")
        do_center(text)
      when /^--right /
        text = line.sub(/^--right /,"")
        do_right(text)
      when /^--exec /
        cmdline = line.sub(/^--exec /,"")
        do_exec(cmdline)
      when /^---/
        return true
      when /^--beginoutput/
        do_beginoutput
      when /^--beginshelloutput/
        do_beginshelloutput
      when /^--endoutput/
        do_endoutput
      when /^--endshelloutput/
        do_endshelloutput
      when /^--sleep /
        time2sleep = line.sub(/^--sleep /,"")
        do_sleep(time2sleep)
      when /^--boldon/
        do_boldon
      when /^--boldoff/
        do_boldoff
      when /^--revon/
        do_revon
      when /^--revoff/
        do_revoff
      when /^--ulon/
        do_ulon
      when /^--uloff/
        do_uloff
      when /^--beginslideleft/
        do_beginslideleft
      when /^--endslideleft/, /^--endslideright/, /^--endslidetop/, /^--endslidebottom/
        do_endslide
      when /^--beginslideright/
        do_beginslideright
      when /^--beginslidetop/
        do_beginslidetop
      when /^--beginslidebottom/
        do_beginslidebottom
      when /^--sethugefont /
        params = line.sub(/^--sethugefont /,"")
        do_sethugefont(params.strip)
      when /^--huge /
        figlet_text = line.sub(/^--huge /,"")
        do_huge(figlet_text)
      when /^--footer /
        @footer_txt = line.sub(/^--footer /,"")
        do_footer(@footer_txt)
      when /^--header /
        @header_txt = line.sub(/^--header /,"")
        do_header(@header_txt)
      when /^--title /
        title = line.sub(/^--title /,"")
        do_title(title)
      when /^--author /
        author = line.sub(/^--author /,"")
        do_author(author)
      when /^--date /
        date = line.sub(/^--date /,"")
        if date == "today" then
          date = Time.now.strftime("%b %d %Y")
        elsif date =~ /^today / then
          date = Time.now.strftime(date.sub(/^today /,""))
        end
        do_date(date)
      when /^--bgcolor /
        color = line.sub(/^--bgcolor /,"").strip
        do_bgcolor(color)
      when /^--fgcolor /
        color = line.sub(/^--fgcolor /,"").strip
        do_fgcolor(color)
      when /^--color /
        color = line.sub(/^--color /,"").strip
        do_color(color)
      when /^--include-file /
        @lastFileName = line.sub(/^--include-file /,"").strip
        do_beginoutput
        print_line(@lastFileName)
        do_beginoutput
        f = File.open(@lastFileName)
        f.each_line do |fileLine|
          fileLine.chomp!
          if fileLine
            print_line(fileLine)
          end
        end
        do_endoutput
    else
      print_line(line)
    end

    return false
  end
end

class InteractiveController
  def initialize(filename,visualizer_class)
    @filename = filename
    @vis = visualizer_class.new
    @cur_page = 0
  end

  def close
    @vis.close
  end

  def run
    begin
      @reload_file = false
      parser = FileParser.new(@filename)
      @pages = parser.get_pages
      if @cur_page >= @pages.size then
        @cur_page = @pages.size - 1
      end
      @vis.clear
      @vis.new_page(@pages[@cur_page].title)
      do_run
    end while @reload_file
  end

  def do_run
    loop do
      wait = false
      @vis.draw_slidenum(@cur_page + 1, @pages.size, false)
      # read and visualize lines until the visualizer says "stop" or we reached end of page
      begin
        line = @pages[@cur_page].next_line
        eop = @pages[@cur_page].eop?
        wait = @vis.visualize(line)
      end while not wait and not eop
      # draw slide number on the bottom left and redraw:
      @vis.draw_slidenum(@cur_page + 1, @pages.size, eop)
      @vis.do_refresh

      # read a character from the keyboard
      # a "break" in the when means that it breaks the loop, i.e. goes on with visualizing lines
      loop do
        ch = @vis.get_key
        case ch
          when :quit
            return
          when :lastpage
            @cur_page = @pages.size - 1
            break
          when :edit
            if @vis.getLastFile
              screen = @vis.store_screen
              Kernel.system("vim " + @vis.getLastFile)
              @vis.restore_screen(screen)
              @reload_file = true
              return
            end
            break
          when :firstpage
            @cur_page = 0
            break
          when :jumptoslide
            screen = @vis.store_screen
            p = @vis.read_newpage(@pages,@cur_page)
            if p >= 0 and p < @pages.size
              @cur_page = p
              @pages[@cur_page].reset_eop
              @vis.new_page(@pages[@cur_page].title)
            else
              @vis.restore_screen(screen)
            end
            break
          when :reload
            @reload_file = true
            return
          when :help
            screen = @vis.store_screen
            @vis.show_help_page
            ch = @vis.get_key
            @vis.clear
            @vis.restore_screen(screen)
          when :keyright
            if @cur_page + 1 < @pages.size and eop then
              @cur_page += 1
              @pages[@cur_page].reset_eop
              @vis.new_page(@pages[@cur_page].title)
            end
            break
          when :keyleft
            if @cur_page > 0 then
              @cur_page -= 1
              @pages[@cur_page].reset_eop
              @vis.new_page(@pages[@cur_page].title)
            end
            break
          when :keyresize
            @vis.setsizes
        end
      end
    end # loop
  end
end

# Prints a nicely formatted usage message.
def usage
  $stderr.puts "usage: #{$0} [-t <type> -o <file>] <file>\n"
  $stderr.puts "\t --version\tprint the version"
  $stderr.puts "\t --help\t\tprint this help"
  Kernel.exit(1)
end

################################
# Here starts the main program #
################################

input = nil

skip_next = false

ARGV.each_index do |i|
  if skip_next then
    skip_next = false
  else
    if ARGV[i] == '-v' or ARGV[i] == '--version' then
      printf "tpp - text presentation program %s\n", version_number
      Kernel.exit(1)
    elsif ARGV[i] == '-h' or ARGV[i] == '--help' then
      usage
    elsif input == nil then
      input = ARGV[i]
    end
  end
end

if input == nil then
  usage
end

load_ncurses
ctrl = InteractiveController.new(input,NcursesVisualizer)

begin
  ctrl.run
ensure
  ctrl.close
end
