from kbhit import KBHit
import scope

bright = 1.0

kb = KBHit()
scope.start()
while scope.poll():
    if kb.kbhit():
        c = kb.getch()
        if c=="q" or "w":
            bright += 0.1 if c=="w" else -0.1
            scope.set_bright(bright)
            print ("bright %f" % bright)
    pass

scope.stop()
