SRC_FILES = \
	../Players/main.cpp \
	../Players/SceneDrawer.cpp \
	../Players/kbhit.cpp \
	../Players/signal_catch.cpp

INC_DIRS += ../Players

EXE_NAME = Sample-Players

DEFINES = USE_GLUT
USED_LIBS = glut

include ../NiteSampleMakefile

