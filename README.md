pong game emulator
==================
This is a clone of the pong game, a C implementation with ncurses interface.

![Screenshot](http://i1383.photobucket.com/albums/ah312/m-programmer/pong_zps08d52b70.png)

Technical details
=================
The game main thread acts as a controller, receiving data from three 
children threads: one for the keyboard input handling, one controlling the
ball position and one for the ai moves. Another thread is used as signal listener, handling kill/int/term and terminal resize signals. Signals are blocked during program initialization and then managed with a signal file descriptor and a poll from the kernel. Thread comunication is provided
with a Unix pipe, while threads are provided by user-level pthread library.

Note that ncurses is not thread safe, so operations on the window
must be confined into a critical zone, locked with a mutex.

Note
====
Note: the program uses a system call to change the keyboard settings for a
smooth playing, and previous settings are restored before game exit.
System keyboard settings are managed throug xset command, so the game
requires to run into a X session.

Build
=====
The game requires gcc with ncurses, pthread, unistd, ioctl and signalfd libraries, and the game must run into a Xorg session. So yes, trying to build and run this on Windows is definitely not a good idea.

You can build the game launching the the following command on the project root:
```bash
gcc ./pong.c ./support.c -lpthread -lncurses
```
License
===================
The project is licensed under GPL 3. See [LICENSE](./LICENSE)
file for the full license.
