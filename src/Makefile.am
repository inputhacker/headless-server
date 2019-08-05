bin_PROGRAMS =

bin_PROGRAMS += headless_server

headless_server_CFLAGS = $(HEADLESS_SERVER_CFLAGS)
headless_server_LDADD  = $(HEADLESS_SERVER_LIBS)

headless_server_SOURCES = headless_server.c \
			  debug/debug.c \
			  input/input.c \
			  output/output_led.c \
			  output/HL_UI_LED_APA102.c \
			  output/boot_anim.c \
			  shell/shell.c
