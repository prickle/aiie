Aiie!
=====

Aiie! is an Apple //e emulator, written ground-up for the Teensy
3.6.

The name comes from a game I used to play on the Apple //e back
around 1986 - Ali Baba and the Forty Thieves, published by Quality
Software in 1981.

http://crpgaddict.blogspot.com/2013/07/game-103-ali-baba-and-forty-thieves-1981.html

When characters in the game did damage to each other, they exclaimed
something like "HAH! JUST A SCRATCH!" or "AAARGH!" or "OH MA, I THINK
ITS MY TIME" [sic]. One of these exclamations was "AIIYEEEEE!!"

Build log:
----------

  https://hackaday.io/project/19925-aiie-an-embedded-apple-e-emulator

Getting the ROMs
================

As with many emulators, you have to go get the ROMs yourself. I've got
the ROMs that I dumped out of my Apple //e. You can probably get yours
a lot easier.

There are two files that you'll need:

* apple2e.rom  -- a 32k dump of the entire Apple //e ROM
* disk.rom     -- a 256 byte dump of the DiskII controller ROM (16-sector P5)

The MD5 sums of those two files are:

* 003a780b461c96ae3e72861ed0f4d3d9 apple2e.rom
* 2020aa1413ff77fe29353f3ee72dc295 disk.rom

From those, the appropriate headers will be automatically generated by
"make roms" (or any other target that relies on the ROMs).

Building (on a Mac)
===================

While this isn't the purpose of the emulator, it is functional, and is
my first test target for most of the work. With MacOS 10.11.6 and
Homebrew, you can build and run it like this:

<pre>
  $ make opencv
  $ ./aiie-opencv /path/to/disk.dsk
</pre>

As the name implies, this requires that OpenCV is installed and in
/usr/local/lib. I've done that with Homebrew like this

<pre>
  $ brew install opencv
</pre>

"Why OpenCV?" you might ask. Well, it's just because I had code from
another project lying around that directly manipulated OpenCV bitmap
data. It's functional, and the Mac build is only about functional
testing (for me).

VM
==

The virtual machine architecture is broken in half - the virtual and
physical pieces. There's the root VM object (vm.h), which ties
together the MMU, virtual keyboard, and virtual display.

Then there are the physical interfaces, which aren't as well
organized. They exist as globals in globals.cpp:

<pre>
	   FileManager *g_filemanager = NULL;
	   PhysicalDisplay *g_display = NULL;
	   PhysicalKeyboard *g_keyboard = NULL;
	   PhysicalSpeaker *g_speaker = NULL;
	   PhysicalPaddles *g_paddles = NULL;
</pre>

There are the two globals that point to the VM and the virtual CPU:

<pre>
	   Cpu *g_cpu = NULL;
	   VM *g_vm = NULL;
</pre>

And there are two global configuration values that probably belong in
some sort of Prefs class:

<pre>
	   int16_t g_volume;
	   uint8_t g_displayType;
</pre>


CPU
===

The CPU is a 65C02, not quite complete; it supports all of the 65C02
documented opcodes but not the undocumented ones here:

	   http://www.oxyron.de/html/opcodes02.html

The timing of the CPU is also not quite correct. It's close, but
doesn't count cycles due to page boundary crossings during branch
instructions. (See the "cycle count footnotes" in cpu.cpp.)

The CPU passes the 6502 functional test from here:

    https://github.com/Klaus2m5/6502_65C02_functional_tests

... which is included in binary form in the test harness (see the .h
files in util/ for notes).

testharness.basic should reach "test number 240", hang for a while,
and then exit.

testharness.verbose should show that it gets through 43 tests, test
240, and then loops repeatedly for a while (exiting at a somewhat
arbitrary point).

testharness.extended currently fails (hanging at 0x733) because I
haven't implemented the undocumented opcodes. It should get to address
0x24a8 and hang. Some day I'll finish implementing all of the
undocumented opcodes :)

