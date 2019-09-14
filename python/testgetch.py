import sys, termios, tty

# unix is so pretty
def getch():
        import sys, tty, termios, select
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        new_term = termios.tcgetattr(fd)
        try:
            #tty.setraw(sys.stdin.fileno())
            new_term[3] = (new_term[3] & ~termios.ICANON & ~termios.ECHO)
            termios.tcsetattr(fd, termios.TCSAFLUSH, new_term)
            #tty.setcbreak(sys.stdin.fileno(), termios.TCSANOW)
            ch = sys.stdin.read(1) # if select.select([sys.stdin], [], [], 0)[0] != [] else None
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        return ch

while True:
    print ("q %s" % getch())
