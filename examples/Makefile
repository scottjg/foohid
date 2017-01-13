all: virtual_keyboard virtual_mouse virtual_u2f

virtual_keyboard: keyboard.c
	gcc keyboard.c -o virtual_keyboard -framework IOKit

virtual_mouse: mouse.c
	gcc mouse.c -o virtual_mouse -framework IOKit

virtual_u2f: u2f.c
	gcc u2f.c -o virtual_u2f -framework IOKit
