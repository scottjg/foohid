all: keyboard mouse u2f

keyboard: keyboard.c
	gcc keyboard.c -o virtual_keyboard -framework IOKit

mouse: mouse.c
	gcc mouse.c -o virtual_mouse -framework IOKit

u2f: u2f.c
	gcc u2f.c -o virtual_u2f -framework IOKit